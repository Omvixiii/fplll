// SPDX-FileCopyrightText: 2015-2016 Martin Albrecht, Leo Ducas
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "../detail/float_type.h"
#include <fplll/defs.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/nr/numvect.h>
#include <fplll/pruner/pruner.h>

#include <algorithm>  // IWYU pragma: keep
#include <array>
#include <bit>
#include <cmath>  // IWYU pragma: keep
#include <cstddef>
#include <cstdint>
#include <gmp.h>
#include <limits>
#include <mpfr.h>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

// Implementation-include components; these are not separate translation units.
#include "pruner_cost.inl"
#include "pruner_optimize.inl"
#include "pruner_optimize_tc.inl"
#include "pruner_optimize_tp.inl"
#include "pruner_prob.inl"
#include "pruner_util.inl"

namespace fplll
{

namespace
{

/* --- Tabulated constants: compile-time tables for the double backend (PRU-01)
   The tables below hold every value that backend used to compute on the first
   Pruner construction: 0! .. 2046! and the volume of the unit ball in
   dimensions 0 .. 2046.  Both are constants of the algorithm, so they are now
   materialised at compile time and only copied into the tables instead of
   being recomputed with GMP/MPFR (about 58 ms and 5.3 MB of transient decimal
   strings).  The 453 ball-volume literals are the non-zero values produced by
   replaying the MPFR loop that is still used for the other backends (256-bit
   pi^(i/2) / Gamma(i/2 + 1), rounded to double); from dimension 453 on every
   value underflows to +0.0, so the initializer stops there and the aggregate
   zero-fills the remaining entries.  The table is regenerated with
   scripts/gen_pruner_ball_volumes.cpp (see its header for the command), and the
   unit test compares all 2047 entries of both tables bit for bit against the
   historical runtime path, so an MPFR change cannot silently alter the
   values. */

/** Exact unsigned integer in base 2^32, little-endian, for the compile-time
    factorial table.  170! is the last factorial that fits a double (171!
    overflows), and it needs 1020 bits, so 40 limbs of 32 bits suffice. */
struct CompileTimeBig
{
  std::array<std::uint32_t, 40> limbs{};
  std::size_t used = 1;  ///< Number of limbs in use; limbs[used - 1] != 0
};

/** Multiplies `big` in place by a small factor. */
constexpr void mul_small(CompileTimeBig &big, std::uint32_t factor) noexcept
{
  std::uint64_t carry = 0;
  for (std::size_t i = 0; i < big.used; ++i)
  {
    const std::uint64_t product = static_cast<std::uint64_t>(big.limbs[i]) * factor + carry;
    big.limbs[i]                = static_cast<std::uint32_t>(product);
    carry                       = product >> 32;
  }
  while (carry != 0)
  {
    big.limbs[big.used++] = static_cast<std::uint32_t>(carry);
    carry >>= 32;
  }
}

/** Number of significant bits of `big`. */
constexpr unsigned bit_length(const CompileTimeBig &big) noexcept
{
  return static_cast<unsigned>((big.used - 1) * 32) +
         static_cast<unsigned>(32 - std::countl_zero(big.limbs[big.used - 1]));
}

/** Exact round-to-nearest-even conversion of `big` to double. */
constexpr double to_double(const CompileTimeBig &big) noexcept
{
  const unsigned nbits = bit_length(big);
  if (nbits > 1024)
  {
    return std::numeric_limits<double>::infinity();
  }
  if (nbits <= 64)
  {
    std::uint64_t value = 0;
    for (std::size_t i = big.used; i-- > 0;)
    {
      value = (value << 32) | big.limbs[i];
    }
    return static_cast<double>(value);  // Exact, hence correctly rounded
  }
  // Keep the top 64 bits in `window`; the bits below are the sticky bit.
  const unsigned shift  = nbits - 64;
  const unsigned limb_i = shift / 32;
  const unsigned offset = shift % 32;
  const unsigned __int128 window =
      (static_cast<unsigned __int128>(big.limbs[limb_i]) |
       (static_cast<unsigned __int128>(limb_i + 1 < big.used ? big.limbs[limb_i + 1] : 0u) << 32) |
       (static_cast<unsigned __int128>(limb_i + 2 < big.used ? big.limbs[limb_i + 2] : 0u) << 64));
  const auto top = static_cast<std::uint64_t>(window >> offset);

  bool sticky = false;
  for (unsigned i = 0; i < limb_i && !sticky; ++i)
  {
    sticky = big.limbs[i] != 0;
  }
  for (unsigned bit = 0; bit < offset && !sticky; ++bit)
  {
    sticky = ((big.limbs[limb_i] >> bit) & 1u) != 0;
  }

  // Round the 64-bit window to 53 bits, nearest-even, then normalize.
  std::uint64_t mantissa        = top >> 11;
  const std::uint64_t remainder = top & 0x7ffu;
  if (remainder > 0x400u || (remainder == 0x400u && (sticky || (mantissa & 1u) != 0)))
  {
    ++mantissa;
  }
  unsigned exponent_shift = shift + 11;
  if (mantissa == (std::uint64_t{1} << 53))
  {
    mantissa >>= 1;
    ++exponent_shift;
  }
  if (exponent_shift > 1023 - 52)
  {
    return std::numeric_limits<double>::infinity();
  }
  const std::uint64_t bits = (static_cast<std::uint64_t>(exponent_shift + 52 + 1023) << 52) |
                             (mantissa & ((std::uint64_t{1} << 52) - 1));
  return std::bit_cast<double>(bits);
}

/** The table the double backend used to build at start-up: i! rounded to
    double, or infinity from 171! on (it overflows a double there). */
consteval std::array<double, max_pruner_dim> make_factorial_table() noexcept
{
  constexpr std::size_t first_overflow = 171;  // 170! < DBL_MAX < 171!
  std::array<double, max_pruner_dim> table{};
  CompileTimeBig exact;
  exact.limbs[0] = 1;  // 0! = 1
  for (std::size_t i = 0; i < first_overflow; ++i)
  {
    if (i > 0)
    {
      mul_small(exact, static_cast<std::uint32_t>(i));
    }
    table[i] = to_double(exact);
  }
  for (std::size_t i = first_overflow; i < max_pruner_dim; ++i)
  {
    table[i] = std::numeric_limits<double>::infinity();
  }
  return table;
}

inline constexpr std::array<double, max_pruner_dim> factorial_double = make_factorial_table();

// Volume of the unit ball in dimension i, pi^(i/2) / Gamma(i/2 + 1),
// for the double backend.  Generated once from the 256-bit MPFR
// computation that set_tabulated_consts() used to run at start-up
// (same operations and rounding as the mpfr backend below), then
// rounded to double; every entry was checked bit for bit against the
// runtime path.  The values underflow to +0.0 from dimension 453 on,
// so the initializer stops there and zero-fills the rest.
inline constexpr std::array<double, max_pruner_dim> ball_vol_double = {
    0x1p+0,
    0x1p+1,
    0x1.921fb54442d18p+1,
    0x1.0c152382d7366p+2,
    0x1.3bd3cc9be45dep+2,
    0x1.50e1eb50f3976p+2,
    0x1.4abbce625be53p+2,
    0x1.2e6290cef4eedp+2,
    0x1.03c1f081b5ac4p+2,
    0x1.a6358a2b69ee8p+1,
    0x1.466bc6775aae2p+1,
    0x1.e254a1c03f10dp+0,
    0x1.55d3c7e3cbffap+0,
    0x1.d23deea12eccap-1,
    0x1.32d2cce62bd86p-1,
    0x1.86991141a8ff6p-2,
    0x1.e1f506891babbp-3,
    0x1.20bab3dc8101bp-3,
    0x1.50783487ee782p-4,
    0x1.7dec9576c3b57p-5,
    0x1.a6d1f2a204a8cp-6,
    0x1.c915f468284fep-7,
    0x1.e3074fde8871fp-8,
    0x1.f378691d9b076p-9,
    0x1.f9d38a3763cc3p-10,
    0x1.f61f536576352p-11,
    0x1.e8f434d018d63p-12,
    0x1.d3658f946c006p-13,
    0x1.b6e24f44b128fp-14,
    0x1.95114f4a6c2b5p-15,
    0x1.6fadb9f155744p-16,
    0x1.4866e45924c72p-17,
    0x1.20c62c2f2d7f5p-18,
    0x1.f4387bb45086ep-20,
    0x1.aaec32af93359p-21,
    0x1.6732959deb1dap-22,
    0x1.2a0c591af8314p-23,
    0x1.e7fabe9fb0b1bp-25,
    0x1.8a404211f9547p-26,
    0x1.3a77df45d86e6p-27,
    0x1.ef6e308d6d1c4p-29,
    0x1.8188bb2746531p-30,
    0x1.2877020d52cfp-31,
    0x1.c2acfed570b68p-33,
    0x1.52ae4120fde27p-34,
    0x1.f768df739012p-36,
    0x1.7215f879e1ac9p-37,
    0x1.0d315fac3959dp-38,
    0x1.838d8f43218p-40,
    0x1.14252474b6d07p-41,
    0x1.859c594ba4573p-43,
    0x1.102aff8ba95dap-44,
    0x1.789d662bb5482p-46,
    0x1.022023bf890fbp-47,
    0x1.5e91aac4928dbp-49,
    0x1.d7cfa3d6ddcc8p-51,
    0x1.3aab85bac2365p-52,
    0x1.a0111f2857d56p-54,
    0x1.10b5242e256ccp-55,
    0x1.6278a27f8d027p-57,
    0x1.c8ed0a80ad0c3p-59,
    0x1.2417c8cc79e0cp-60,
    0x1.7271f9271ad31p-62,
    0x1.d219eec01c1d4p-64,
    0x1.22f26dacf7fd9p-65,
    0x1.6871743a311cep-67,
    0x1.bb2b5ea2cae7fp-69,
    0x1.0e6a69c70f12fp-70,
    0x1.47970e6d24716p-72,
    0x1.89fcd79e5d227p-74,
    0x1.d6787c412d0c6p-76,
    0x1.16ede663e1becp-77,
    0x1.487352263c208p-79,
    0x1.801fabeb3dc9p-81,
    0x1.be356c916304ep-83,
    0x1.0171331c2d1cep-84,
    0x1.271df64666882p-86,
    0x1.501da0a083095p-88,
    0x1.7c5d366adee2fp-90,
    0x1.abb8d5b9948e1p-92,
    0x1.ddfac9a6fd2a3p-94,
    0x1.096d85794c167p-95,
    0x1.24ff9dd79302ep-97,
    0x1.417d93e4e4f17p-99,
    0x1.5ea8e7d4f178bp-101,
    0x1.7c3b7e8ef33f9p-103,
    0x1.99e8aebb138dbp-105,
    0x1.b75e8b2961b3ep-107,
    0x1.d44762c42cde9p-109,
    0x1.f04b6bec7718dp-111,
    0x1.058947940953cp-112,
    0x1.122321c3e2fffp-114,
    0x1.1dc9b71a62b83p-116,
    0x1.28561b1a954a8p-118,
    0x1.31a4deda2a1c3p-120,
    0x1.3996d979e2619p-122,
    0x1.4011d6386d1e4p-124,
    0x1.450122d913168p-126,
    0x1.4855fa3e5ea4ap-128,
    0x1.4a07c86535ad1p-130,
    0x1.4a14464f1772ep-132,
    0x1.487f6dd22fcd6p-134,
    0x1.455346a3cd957p-136,
    0x1.409f8f36b5489p-138,
    0x1.3a794525c903p-140,
    0x1.32fa11d204094p-142,
    0x1.2a3fa09116963p-144,
    0x1.206ae4435addep-146,
    0x1.159f5262d6b3cp-148,
    0x1.0a02199af1da9p-150,
    0x1.fb72b37b2d8e5p-153,
    0x1.e1d6c4fa08cfcp-155,
    0x1.c77bfb73c0b98p-157,
    0x1.acab92f91908p-159,
    0x1.91ab49c2fde63p-161,
    0x1.76bc64c938ae1p-163,
    0x1.5c1ae23264db6p-165,
    0x1.41fcdb085f718p-167,
    0x1.28921408de98p-169,
    0x1.1003bbffa03f4p-171,
    0x1.f0e8a9c0c330ap-174,
    0x1.c3ff87b216bf8p-176,
    0x1.997707a6f39acp-178,
    0x1.716deab4b2834p-180,
    0x1.4bf78d16eecap-182,
    0x1.291cdb5e68d74p-184,
    0x1.08dd6254a4864p-186,
    0x1.d660df17a0a2ap-189,
    0x1.a00c72c2f9109p-191,
    0x1.6e920c7617fc7p-193,
    0x1.41bc97c8c3d1dp-195,
    0x1.194f8259df639p-197,
    0x1.ea11745cdf38bp-200,
    0x1.a945007d3eec1p-202,
    0x1.6faa14f0b085dp-204,
    0x1.3cafe51823637p-206,
    0x1.0fc6e152b89b5p-208,
    0x1.d0c58b7ad1255p-211,
    0x1.8bf88c61a33abp-213,
    0x1.5024baf430aa6p-215,
    0x1.1c568e9725d69p-217,
    0x1.df54b5983b9c7p-220,
    0x1.929a44425f516p-222,
    0x1.50f9e8a57b19p-224,
    0x1.1911d3f94b928p-226,
    0x1.d3433df6cc101p-229,
    0x1.83123e487ef96p-231,
    0x1.3f8db311ee914p-233,
    0x1.06ec5d02cebcap-235,
    0x1.af35574ca8c35p-238,
    0x1.606cff042d5b6p-240,
    0x1.1f15b043a9d91p-242,
    0x1.d22e27beb78e8p-245,
    0x1.794447b0ecf25p-247,
    0x1.305260740bcacp-249,
    0x1.e96167352ac6dp-252,
    0x1.883a4c3dc844cp-254,
    0x1.395cc43b3fab3p-256,
    0x1.f3208208250c4p-259,
    0x1.8c4267412a202p-261,
    0x1.399c5904a4cep-263,
    0x1.eedc8166e9086p-266,
    0x1.853ac09d6bfeap-268,
    0x1.3135397185bdcp-270,
    0x1.dd30c1b42d4b1p-273,
    0x1.73e9cc9ce2a88p-275,
    0x1.20fd8f05d2a9p-277,
    0x1.bfc52339f38e9p-280,
    0x1.59dcec215e06cp-282,
    0x1.0a5c08cd45898p-284,
    0x1.990edff5c4f7fp-287,
    0x1.392f8ae7ac58bp-289,
    0x1.de2cab42b66bfp-292,
    0x1.6bfca1c49e89fp-294,
    0x1.1445a754139a9p-296,
    0x1.a23196327b0ffp-299,
    0x1.3b9ccc3a54474p-301,
    0x1.db0b53de07ddbp-304,
    0x1.6480f1bca6b57p-306,
    0x1.0acc05e05aa64p-308,
    0x1.8e37ebd692614p-311,
    0x1.285e5b1dfd868p-313,
    0x1.b7ed1b7bf018p-316,
    0x1.459eb083673afp-318,
    0x1.e0b8394d250e8p-321,
    0x1.61e3fe96a768ap-323,
    0x1.03d2ddd2c1e05p-325,
    0x1.7c8095e28afe1p-328,
    0x1.15e035adbe633p-330,
    0x1.94c92cf147e1cp-333,
    0x1.260dcef48365dp-335,
    0x1.aa1c2d455c748p-338,
    0x1.33eebc94211ffp-340,
    0x1.bbe8cdbda4022p-343,
    0x1.3f244f39a1b28p-345,
    0x1.c9b588eb7008bp-348,
    0x1.476250d61d4a9p-350,
    0x1.d325873320716p-353,
    0x1.4c72620a3a992p-355,
    0x1.d7fca659e419cp-358,
    0x1.4e36449bcf2f9p-360,
    0x1.d821e0ced40e2p-363,
    0x1.4ca8f165addbbp-365,
    0x1.d39ff449ae2a4p-368,
    0x1.47de6dd9c1b22p-370,
    0x1.caa4469cc33b8p-373,
    0x1.40026b3d79b2fp-375,
    0x1.bd7c260bd8f03p-378,
    0x1.3555ce5409902p-380,
    0x1.ac909d87360bfp-383,
    0x1.282b4f7ed787fp-385,
    0x1.98612aab06008p-388,
    0x1.18e3702c51673p-390,
    0x1.817db1d62045cp-393,
    0x1.07e809ea65392p-395,
    0x1.6880134f7152bp-398,
    0x1.eb4f7ab4fb191p-401,
    0x1.4e05c34c8b686p-403,
    0x1.c52307bf900b2p-406,
    0x1.32a9be79905a6p-408,
    0x1.9e213dd366a8ep-411,
    0x1.16ff26b080b23p-413,
    0x1.7712288f35e05p-416,
    0x1.f7198319fe2eap-419,
    0x1.50a9b28d830d6p-421,
    0x1.c192fcb1fe563p-424,
    0x1.2b837f9ca725cp-426,
    0x1.8e342890dd09p-429,
    0x1.082054fde9fa8p-431,
    0x1.5d9f6d8a4ffd8p-434,
    0x1.cdca0071de5fp-437,
    0x1.304f99f993111p-439,
    0x1.90353013c3067p-442,
    0x1.069919c41bcc2p-444,
    0x1.57dfc6bc43f35p-447,
    0x1.c159437949e26p-450,
    0x1.24f7512c435ep-452,
    0x1.7d35ea5ca83p-455,
    0x1.eefe8cc0e07bdp-458,
    0x1.40b2d096d7376p-460,
    0x1.9eaf7bea9ac57p-463,
    0x1.0b8d8d6d7223fp-465,
    0x1.5888f414a826bp-468,
    0x1.bac13bb495824p-471,
    0x1.1be7baa948181p-473,
    0x1.6b5a01638491fp-476,
    0x1.d015e846bbab9p-479,
    0x1.27c622e824c54p-481,
    0x1.783ff084deae2p-484,
    0x1.dda9670e1b1a7p-487,
    0x1.2e991778fceefp-489,
    0x1.7ea0ba6bfe5ecp-492,
    0x1.e2dd5704d5695p-495,
    0x1.301420f3c4bfcp-497,
    0x1.7e3a24f71e441p-500,
    0x1.df84ca4860ddfp-503,
    0x1.2c3352ddc765fp-505,
    0x1.7725cf522108ep-508,
    0x1.d3e5faf1be10dp-511,
    0x1.233a3cac65fc8p-513,
    0x1.69d5373eb3e0ap-516,
    0x1.c0b20b514eddep-519,
    0x1.15acc44978472p-521,
    0x1.570667af7d20ep-524,
    0x1.a6f4104242d48p-527,
    0x1.0442eb76660dfp-529,
    0x1.3fb2d5ed67ca2p-532,
    0x1.87f99894c1779p-535,
    0x1.dfb2140527194p-538,
    0x1.24fa865cd1099p-540,
    0x1.65377df175e4ep-543,
    0x1.b2bca3370bd0cp-546,
    0x1.080dd3f7037aap-548,
    0x1.402dfad3b0da8p-551,
    0x1.8386eeb5f2c3dp-554,
    0x1.d43046e4b41bap-557,
    0x1.1a4ec98c390cfp-559,
    0x1.53d651a4d37c7p-562,
    0x1.985ae09fd81bep-565,
    0x1.e9cf09e62e41cp-568,
    0x1.253b1a009d5fap-570,
    0x1.5e782848b8301p-573,
    0x1.a22385197fd61p-576,
    0x1.f1fe46873719fp-579,
    0x1.2807043a7c9fcp-581,
    0x1.5f5313621b13bp-584,
    0x1.a03900fbbf769p-587,
    0x1.ec40322f1d51dp-590,
    0x1.229407a0c4b8p-592,
    0x1.56777c8369bfdp-595,
    0x1.92ecf8a201b56p-598,
    0x1.d93e80dde5b9bp-601,
    0x1.15711b5d9601fp-603,
    0x1.44bfade12c0e6p-606,
    0x1.7b79e27f52757p-609,
    0x1.baace1cd76139p-612,
    0x1.01c3a1c8f6419p-614,
    0x1.2bae3f564dc0bp-617,
    0x1.5bd44cadb6b12p-620,
    0x1.930a0d7d42a39p-623,
    0x1.d23c1feb73776p-626,
    0x1.0d38dbc2b9803p-628,
    0x1.36677c694b0bap-631,
    0x1.654bba632b9dep-634,
    0x1.9a98616b3cafbp-637,
    0x1.d7129bfd936f7p-640,
    0x1.0dc9ae14ab6cp-642,
    0x1.348468228b035p-645,
    0x1.603bf0e222b53p-648,
    0x1.917ede0a8cacfp-651,
    0x1.c8e8ac88bb812p-654,
    0x1.0391440b32e5dp-656,
    0x1.2672094900d6fp-659,
    0x1.4d7a14a35ea6cp-662,
    0x1.7914d30f69106p-665,
    0x1.a9b60d42dd70cp-668,
    0x1.dfda40c5b170fp-671,
    0x1.0e0379a985edp-673,
    0x1.2f657f8feae7cp-676,
    0x1.545f6b42ec23bp-679,
    0x1.7d425ebe7bb6dp-682,
    0x1.aa647b9c8d1c6p-685,
    0x1.dc20f319950a5p-688,
    0x1.096c0735b6a87p-690,
    0x1.2777974186bbbp-693,
    0x1.48686127ad9c7p-696,
    0x1.6c76194bd1eeep-699,
    0x1.93dae143abe4p-702,
    0x1.bed332054fc91p-705,
    0x1.ed9dbf9245fafp-708,
    0x1.103daadd09b49p-710,
    0x1.2bd756cb6cf87p-713,
    0x1.49be1db8465cap-716,
    0x1.6a14fa5f7239p-719,
    0x1.8cff8707e7001p-722,
    0x1.b2a1d853bcc53p-725,
    0x1.db2053e0375d3p-728,
    0x1.034fc1594335dp-730,
    0x1.1aa1efbc22756p-733,
    0x1.3398d056ee927p-736,
    0x1.4e463f6461434p-739,
    0x1.6abbbb47ebfc1p-742,
    0x1.890a43ec9da79p-745,
    0x1.a9423878f6791p-748,
    0x1.cb733380a128dp-751,
    0x1.efabe5e3cdb11p-754,
    0x1.0afcf849513b9p-756,
    0x1.1f34deb8e7b6ap-759,
    0x1.34832bd1d5f89p-762,
    0x1.4aeca1bfa9fb3p-765,
    0x1.62752b426d89ep-768,
    0x1.7b1fc7d165e83p-771,
    0x1.94ee7812d779ep-774,
    0x1.afe22ad715128p-777,
    0x1.cbfaaac6a830ep-780,
    0x1.e9368cf4766d8p-783,
    0x1.03c99042b39d8p-785,
    0x1.13862fcf95a8ep-788,
    0x1.23ce70e7aed1ap-791,
    0x1.349ee80d9f625p-794,
    0x1.45f36d7361b57p-797,
    0x1.57c718efc93a8p-800,
    0x1.6a143f0aef668p-803,
    0x1.7cd46f34e3475p-806,
    0x1.9000733807892p-809,
    0x1.a3904ff65d1cbp-812,
    0x1.b77b477e8a373p-815,
    0x1.cbb7dc82c2e9ep-818,
    0x1.e03bd738d7e89p-821,
    0x1.f4fc4ba7a003ep-824,
    0x1.04f6d0b15c7bfp-826,
    0x1.0f81ce5922724p-829,
    0x1.1a18b4906e0fbp-832,
    0x1.24b4d2b3385d6p-835,
    0x1.2f4f3851b216dp-838,
    0x1.39e0bc6dcd383p-841,
    0x1.4462054ef995ap-844,
    0x1.4ecb90e0a5f59p-847,
    0x1.5915bd8e88173p-850,
    0x1.6338d39037dc9p-853,
    0x1.6d2d0e9451671p-856,
    0x1.76eaa7ba1befap-859,
    0x1.8069dfc7a3c5bp-862,
    0x1.89a3098950ff7p-865,
    0x1.928e94474d59dp-868,
    0x1.9b25163c8564ep-871,
    0x1.a35f56fabd998p-874,
    0x1.ab3659a714f5ap-877,
    0x1.b2a366fa65324p-880,
    0x1.b9a016f13cdbbp-883,
    0x1.c0265a17b1285p-886,
    0x1.c630825dfb47ep-889,
    0x1.cbb94b63bd55ap-892,
    0x1.d0bbe228e3b6cp-895,
    0x1.d533ec145d80ap-898,
    0x1.d91d8d4255f72p-901,
    0x1.dc756e0d2e339p-904,
    0x1.df38bfc62c008p-907,
    0x1.e1654094a4fdap-910,
    0x1.e2f93e7455df4p-913,
    0x1.e3f3994d8f8bfp-916,
    0x1.e453c423e93edp-919,
    0x1.e419c55a31cf5p-922,
    0x1.e346360c64f9ep-925,
    0x1.e1da40826e2d6p-928,
    0x1.dfd79dc07b4d5p-931,
    0x1.dd40923b898eep-934,
    0x1.da17e9baa7ee3p-937,
    0x1.d660f26f20d57p-940,
    0x1.d21f774f5307ep-943,
    0x1.cd57b9c174bacp-946,
    0x1.c80e6aa4c78cp-949,
    0x1.c248a2c8e5c43p-952,
    0x1.bc0bdae3c6b4ep-955,
    0x1.b55de317df41dp-958,
    0x1.ae44da1c5b8fp-961,
    0x1.a6c72419d6d9bp-964,
    0x1.9eeb614e2eaa6p-967,
    0x1.96b8648a1771fp-970,
    0x1.8e35299af3649p-973,
    0x1.8568cbb31aac3p-976,
    0x1.7c5a7be248053p-979,
    0x1.731177af39111p-982,
    0x1.6994ffe2c92aap-985,
    0x1.5fec4f93e396bp-988,
    0x1.561e9382a22bfp-991,
    0x1.4c32e1cfcbd4p-994,
    0x1.4230321cae942p-997,
    0x1.381d561e073f2p-1000,
    0x1.2e00f2ab51b81p-1003,
    0x1.23e179527bedap-1006,
    0x1.19c522768f934p-1009,
    0x1.0fb1e7fe7f149p-1012,
    0x1.05ad8097dd2bcp-1015,
    0x1.f77ab71fcf206p-1019,
    0x1.e3cd3a87ef86bp-1022,
    0x1.d05c38524891p-1025,
    0x1.bd30bcd346cp-1028,
    0x1.aa53302ab28p-1031,
    0x1.97cb53c3d2p-1034,
    0x1.85a040eb48p-1037,
    0x1.73d8686f8p-1040,
    0x1.6279934p-1043,
    0x1.5188e4p-1046,
    0x1.410ad98p-1049,
    0x1.31035p-1052,
    0x1.21758p-1055,
    0x1.1264p-1058,
    0x1.03dp-1061,
    0x1.eb8p-1065,
    0x1.dp-1068,
    0x1.cp-1071,
    0x1p-1073,
};

}  // namespace

/** Tabulated constants: one table per floating-point type, built once on the
    first construction of a Pruner of that type.  The arrays are namespace-scope
    objects of the library so that the MPFR entries are created at MPFR's
    default precision, exactly as before the tables became lazily filled. */
template <class FT> std::once_flag Pruner<FT>::tabulated_values_flag;
// The tables are namespace-scope objects on purpose: their address and lifetime
// are part of the class contract, and the values are filled lazily under
// call_once.  Constructing the FP_NR elements during static initialization
// happens before main() could catch anything, exactly like any other global
// floating-point object, so the diagnostic is disabled instead of changing how
// the tables are stored.
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
template <class FT> std::array<FT, max_pruner_dim> Pruner<FT>::tabulated_factorial;
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
template <class FT> std::array<FT, max_pruner_dim> Pruner<FT>::tabulated_ball_vol;

template <class FT> void Pruner<FT>::set_tabulated_consts()
{
  std::call_once(tabulated_values_flag,
                 []
                 {
                   if constexpr (std::is_same_v<FT, FP_NR<double>>)
                   {
                     // The whole fill of the double backend is a copy of the
                     // compile-time tables above.
                     for (std::size_t i = 0; i < max_pruner_dim; ++i)
                     {
                       tabulated_factorial[i] = factorial_double[i];
                       tabulated_ball_vol[i]  = ball_vol_double[i];
                     }
                   }
                   else
                   {
                     // Factorials are exact integers: convert them to the backend
                     // directly and let it round, without the historical decimal
                     // string round trip.  dd/qd keep the string path because their
                     // set_z() is not the correctly rounding algorithm of read().
                     Z_NR<mpz_t> factorial;
                     factorial = 1;
                     for (std::size_t i = 0; i < max_pruner_dim; ++i)
                     {
                       if (i > 0)
                       {
                         factorial.mul_ui(factorial, static_cast<unsigned long>(i));
                       }
                       if constexpr (std::is_same_v<FT, FP_NR<mpfr_t>>)
                       {
                         tabulated_factorial[i].set_z(factorial);
                       }
                       else
                       {
                         std::string decimal(mpz_sizeinbase(factorial.get_data(), 10) + 2, '\0');
                         mpz_get_str(decimal.data(), 10, factorial.get_data());
                         tabulated_factorial[i] = decimal.c_str();
                       }
                     }

                     // Volume of the unit ball in dimension i: pi^(i/2) / Gamma(i/2 + 1).
                     mpfr_t pi, value, gamma, tmp;
                     mpfr_inits2(256, pi, value, gamma, tmp, static_cast<mpfr_ptr>(nullptr));
                     mpfr_const_pi(pi, MPFR_RNDN);
                     for (std::size_t i = 0; i < max_pruner_dim; ++i)
                     {
                       mpfr_set_ui(tmp, static_cast<unsigned long>(i), MPFR_RNDN);
                       mpfr_div_2ui(tmp, tmp, 1, MPFR_RNDN);
                       mpfr_pow(value, pi, tmp, MPFR_RNDN);
                       mpfr_add_ui(tmp, tmp, 1, MPFR_RNDN);
                       mpfr_gamma(gamma, tmp, MPFR_RNDN);
                       mpfr_div(value, value, gamma, MPFR_RNDN);

                       tabulated_ball_vol[i].set_mpfr(value);
                     }
                     mpfr_clears(pi, value, gamma, tmp, static_cast<mpfr_ptr>(nullptr));
                     mpfr_free_cache();
                   }
                 });
}

namespace
{

/** Runs the pruner with a fixed floating-point backend. */
template <class FT> void run_pruner_with_float_type(ZZ_mat<mpz_t> &b, const PrunerOptions &options)
{
  // Row exponents are only usable with the plain double backend.
  constexpr bool use_row_expo = std::is_same_v<FT, FP_NR<double>>;

  int start = options.start;
  int end   = options.end.value_or(b.get_rows());
  if (start < 0 || start >= b.get_rows() - 1)
  {
    start = 0;
  }
  if (end <= start || end >= b.get_rows())
  {
    end = b.get_rows();
  }
  const double pre_nodes = std::max(options.pre_nodes, 1.0);
  const int block_size   = end - start;

  PruningParams pruning;
  std::vector<double> r;
  FT root_det, max_dist;
  long max_dist_expo;

  const GSOOptions gso_options{.row_expo = use_row_expo};

  // We check if we can convert the basis to long integers for performance.
  ZZ_mat<long> bl;
  if (convert<long, mpz_t>(bl, b, 10))
  {
    ZZ_mat<long> empty_mat;
    MatGSO<Z_NR<long>, FT> m_gso(bl, empty_mat, empty_mat, gso_options);
    m_gso.update_gso();
    max_dist = m_gso.get_r_exp(start, start, max_dist_expo);
    root_det = m_gso.get_root_det(start, end);
    for (int i = start; i < end; ++i)
    {
      FT x;
      m_gso.get_r(x, i, i);
      r.push_back(x.get_d());
    }
  }
  else
  {
    ZZ_mat<mpz_t> empty_mat;
    MatGSO<Z_NR<mpz_t>, FT> m_gso(b, empty_mat, empty_mat, gso_options);
    m_gso.update_gso();
    max_dist = m_gso.get_r_exp(start, start, max_dist_expo);
    root_det = m_gso.get_root_det(start, end);
    for (int i = start; i < end; ++i)
    {
      FT x;
      m_gso.get_r(x, i, i);
      r.push_back(x.get_d());
    }
  }

  adjust_radius_to_gh_bound(max_dist, max_dist_expo, block_size, root_det, options.gh_factor);
  const double radius_d = max_dist.get_d() * std::pow(2.0, static_cast<double>(max_dist_expo));

  // A target expected number of solutions implies the fixed-probability
  // optimizer: that is the historical contract of the high-level entry point
  // (the target is achieved rather than merely entering the cost model).
  PrunerOptions prune_options = options;
  prune_options.single        = options.single || options.min_probability.has_value();
  const double target         = options.min_probability.value_or(0.9);

  log_stream() << "# Start Pruning" << '\n';
  log_stream() << "# enumeration Radius: " << radius_d << '\n';
  log_stream() << "# preprocessing (num. nodes): " << pre_nodes << '\n';
  log_stream() << "# targeted min. prob: " << options.min_probability.value_or(-1.0) << '\n';
  log_stream() << "# input GSO: " << r << '\n';

  prune<FT>(pruning, radius_d, pre_nodes, r, target, PrunerMetric::expected_solutions,
            prune_options);
  log_stream() << "# optimized pruning coeff: " << '\n' << pruning.coefficients << '\n';

  double cost = 0.;
  for (int i = 0; i < block_size; ++i)
  {
    cost += pruning.detailed_cost[i];
  }
  log_stream() << "# single_enum_cost   = " << cost << '\n';
  log_stream() << "#       succ. prob   = " << pruning.expectation << '\n';
  log_stream() << "# repeated_enum_cost = " << cost / pruning.expectation << '\n';
}

}  // namespace

RedStatus run_pruner(ZZ_mat<mpz_t> &b, const PrunerOptions &options)
{
  if (b.get_rows() == 0 || b.get_cols() == 0)
  {
    return RedStatus::success;
  }

  const FloatType float_type =
      options.float_type == FloatType::automatic ? FloatType::double_precision : options.float_type;
  FPLLL_CHECK(is_supported(float_type), "run_pruner(): unsupported floating-point type {}",
              to_string(float_type));
  FPLLL_CHECK(float_type != FloatType::mpfr || options.precision > 0,
              "run_pruner(): missing precision for floating-point type mpfr");

  switch (float_type)
  {
  case FloatType::double_precision:
    run_pruner_with_float_type<FP_NR<double>>(b, options);
    break;
#ifdef FPLLL_WITH_QD
  case FloatType::dd:
    run_pruner_with_float_type<FP_NR<dd_real>>(b, options);
    break;
  case FloatType::qd:
    run_pruner_with_float_type<FP_NR<qd_real>>(b, options);
    break;
#endif
  case FloatType::mpfr:
  {
    ScopedPrecision precision_guard(static_cast<unsigned int>(options.precision));
    run_pruner_with_float_type<FP_NR<mpfr_t>>(b, options);
    break;
  }
  case FloatType::automatic:
  default:
    FPLLL_ABORT("run_pruner(): unsupported floating-point type");
  }
  return RedStatus::success;
}

PruningParams linear_pruning_params(std::size_t block_size, std::size_t level)
{
  PruningParams pruning;
  std::size_t start_descent = level >= block_size ? 0 : block_size - level;
  if (start_descent < 1)
  {
    start_descent = 1;
  }
  start_descent = std::min(start_descent, block_size);

  pruning.coefficients.assign(block_size, 0.0);
  for (std::size_t k = 0; k < start_descent; ++k)
  {
    pruning.coefficients[k] = 1.0;
  }
  for (std::size_t k = 0; k + start_descent < block_size; ++k)
  {
    pruning.coefficients[start_descent + k] =
        static_cast<double>(block_size - k - 1) / static_cast<double>(block_size);
  }
  pruning.gh_factor   = 1.0;
  pruning.metric      = PrunerMetric::probability_of_shortest;
  pruning.expectation = svp_probability<FP_NR<double>>(pruning.coefficients).get_d();

  return pruning;
}

template <class FT> FT svp_probability(const PruningParams &pruning)
{
  Pruner<FT> pru(static_cast<int>(pruning.coefficients.size()));
  return pru.measure_metric(pruning.coefficients);
}

template <class FT> FT svp_probability(const std::vector<double> &pr)
{
  Pruner<FT> pru(static_cast<int>(pr.size()));
  return pru.measure_metric(pr);
}

template <class FT>
void prune(/*output*/ PruningParams &pruning,
           /*inputs*/ const double enumeration_radius, const double preproc_cost,
           const std::vector<double> &gso_r, const double target, const PrunerMetric metric,
           const PrunerOptions &options)
{
  Pruner<FT> pruner(enumeration_radius, preproc_cost, gso_r, target, metric, options);
  pruner.optimize_coefficients(pruning.coefficients);
  pruning.detailed_cost.clear();
  static_cast<void>(pruner.single_enum_cost(pruning.coefficients, &pruning.detailed_cost));
  pruning.gh_factor   = enumeration_radius / pruner.gaussian_heuristic().get_d();
  pruning.metric      = metric;
  pruning.expectation = pruner.measure_metric(pruning.coefficients);
}

template <class FT>
void prune(/*output*/ PruningParams &pruning,
           /*inputs*/ const double enumeration_radius, const double preproc_cost,
           const std::vector<std::vector<double>> &gso_rs, const double target,
           const PrunerMetric metric, const PrunerOptions &options)
{
  Pruner<FT> pruner(enumeration_radius, preproc_cost, gso_rs, target, metric, options);
  pruner.optimize_coefficients(pruning.coefficients);
  pruning.detailed_cost.clear();
  static_cast<void>(pruner.single_enum_cost(pruning.coefficients, &pruning.detailed_cost));
  pruning.gh_factor   = enumeration_radius / pruner.gaussian_heuristic().get_d();
  pruning.metric      = metric;
  pruning.expectation = pruner.measure_metric(pruning.coefficients);
}

/** Explicit instantiations. */
/* clang-format off */

// DOUBLE
template class Pruner<FP_NR<double>>;
template void prune<FP_NR<double>>(PruningParams &, const double, const double, const std::vector<double> &, const double, const PrunerMetric, const PrunerOptions &);
template void prune<FP_NR<double>>(PruningParams &, const double, const double, const std::vector<std::vector<double>> &, const double, const PrunerMetric, const PrunerOptions &);
template FP_NR<double> svp_probability<FP_NR<double>>(const PruningParams &pruning);
template FP_NR<double> svp_probability<FP_NR<double>>(const std::vector<double> &pr);

// MPFR
template class Pruner<FP_NR<mpfr_t>>;
template void prune<FP_NR<mpfr_t>>(PruningParams &, const double, const double, const std::vector<double> &, const double, const PrunerMetric, const PrunerOptions &);
template void prune<FP_NR<mpfr_t>>(PruningParams &, const double, const double, const std::vector<std::vector<double>> &, const double, const PrunerMetric, const PrunerOptions &);
template FP_NR<mpfr_t> svp_probability<FP_NR<mpfr_t>>(const PruningParams &pruning);
template FP_NR<mpfr_t> svp_probability<FP_NR<mpfr_t>>(const std::vector<double> &pr);

#ifdef FPLLL_WITH_QD

// DD
template class Pruner<FP_NR<dd_real>>;
template void prune<FP_NR<dd_real>>(PruningParams &, const double, const double, const std::vector<double> &, const double, const PrunerMetric, const PrunerOptions &);
template void prune<FP_NR<dd_real>>(PruningParams &, const double, const double, const std::vector<std::vector<double>> &, const double, const PrunerMetric, const PrunerOptions &);
template FP_NR<dd_real> svp_probability<FP_NR<dd_real>>(const PruningParams &pruning);
template FP_NR<dd_real> svp_probability<FP_NR<dd_real>>(const std::vector<double> &pr);

// QD
template class Pruner<FP_NR<qd_real>>;
template void prune<FP_NR<qd_real>>(PruningParams &, const double, const double, const std::vector<double> &, const double, const PrunerMetric, const PrunerOptions &);
template void prune<FP_NR<qd_real>>(PruningParams &, const double, const double, const std::vector<std::vector<double>> &, const double, const PrunerMetric, const PrunerOptions &);
template FP_NR<qd_real> svp_probability<FP_NR<qd_real>>(const PruningParams &pruning);
template FP_NR<qd_real> svp_probability<FP_NR<qd_real>>(const std::vector<double> &pr);

#endif

/* clang-format on */

}  // namespace fplll
