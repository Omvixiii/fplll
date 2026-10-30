// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "wrapper.h"
#include "detail/float_type.h"
#include "hlll/hlll_kernel.h"
#include "lll/lll_kernel.h"

#include <fplll/defs.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/gso/householder.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/hlll.h>
#include <fplll/reduction/lll.h>
#include <fplll/util.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <gmp.h>
#include <limits>
#include <type_traits>

namespace fplll
{

namespace
{

/* prec=53, eta=0.501, dim < dim_double_max [ (delta / 100.0) + 25 ] */
constexpr std::array<double, 75> dim_double_max = {
    0,     26,    29.6,  28.1,  31.1,  32.6,  34.6,  34,    37.7,  38.8,  39.6,  41.8,  40.9,
    43.6,  44.2,  47,    46.8,  50.6,  49.1,  51.5,  52.5,  54.8,  54.6,  57.4,  57.6,  59.9,
    61.8,  62.3,  64.5,  67.1,  68.8,  68.3,  69.9,  73.1,  74,    76.1,  76.8,  80.9,  81.8,
    83,    85.3,  87.9,  89,    90.1,  89,    94.6,  94.8,  98.7,  99,    101.6, 104.9, 106.8,
    108.2, 107.4, 110,   112.7, 114.6, 118.1, 119.7, 121.8, 122.9, 126.6, 128.6, 129,   133.6,
    126.9, 135.9, 139.5, 135.2, 137.2, 139.3, 142.8, 142.4, 142.5, 145.4};

constexpr std::array<double, 10> eta_dep = {1.,       // 0.5
                                            1.,       // 0.55
                                            1.0521,   // 0.6
                                            1.1254,   // 0.65
                                            1.2535,   // 0.7
                                            1.3957,   // 0.75
                                            1.6231,   // 0.8
                                            1.8189,   // 0.85
                                            2.1025,   // 0.9
                                            2.5117};  // 0.95

/** Backend reported for the wrapper's low-level type `F`. */
template <class F> constexpr FloatType float_type_of() noexcept
{
  if constexpr (std::is_same_v<F, double>)
  {
    return FloatType::double_precision;
  }
#ifdef FPLLL_WITH_QD
  else if constexpr (std::is_same_v<F, dd_real>)
  {
    return FloatType::dd;
  }
  else if constexpr (std::is_same_v<F, qd_real>)
  {
    return FloatType::qd;
  }
#endif
  else
  {
    return FloatType::mpfr;
  }
}

/** Precision reported for a low-level call with explicit `precision`. */
template <class F> inline int float_precision(int precision) noexcept
{
  const FloatType type = float_type_of<F>();
  if (type == FloatType::mpfr)
  {
    return precision > 0 ? precision : static_cast<int>(FP_NR<mpfr_t>::get_prec());
  }
  return detail::native_precision(type);
}

}  // namespace

Wrapper::Wrapper(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
                 const LLLParameters &parameters)
    : status(RedStatus::success), b(b), u(u), u_inv(u_inv), lll_parameters(parameters),
      good_prec(0), use_long(false), max_exponent(0), n(0), d(0), last_early_red(0)
{
  n = b.get_cols();
  d = b.get_rows();

  // An empty basis cannot be reduced; skip the backend bookkeeping.  The
  // dimension factor log2(d * n) and l2_min_prec() are undefined for zero
  // rows/columns and evaluating them was a log2(0)-to-long UB (E-5/P2-3).
  if (d == 0 || n == 0)
  {
    good_prec = PREC_DOUBLE;
    return;
  }

  max_exponent = static_cast<int>(
      b.get_max_exp() + static_cast<long>(std::ceil(0.5 * std::log2(static_cast<double>(d) * n))));

  // Computes the parameters required for the proved version
  good_prec = l2_min_prec(d, parameters.delta, parameters.eta, LLL_DEF_EPSILON);
}

// Constructor for HLLL
Wrapper::Wrapper(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
                 const HLLLParameters &parameters)
    : status(RedStatus::success), b(b), u(u), u_inv(u_inv), hlll_parameters(parameters),
      good_prec(0), use_long(false), max_exponent(0), n(0), d(0), last_early_red(-1)
{
  n = b.get_cols();
  d = b.get_rows();

  // See the LLL constructor: an empty basis needs no proved precision and
  // hlll_min_prec() must not see a zero dimension.
  if (d == 0 || n == 0)
  {
    good_prec = PREC_DOUBLE;
    return;
  }

  // Computes the parameters required for the proved version
  good_prec = hlll_min_prec(d, n, parameters.delta, parameters.eta, parameters.theta, parameters.c);
}

bool Wrapper::little(int kappa, int precision)
{
  /* one may add here dimension arguments with respect to eta and delta */
  int dm = static_cast<int>(lll_parameters.delta * 100. - 25.);
  if (dm < 0)
  {
    dm = 0;
  }
  if (dm > 74)
  {
    dm = 74;
  }

  int em = static_cast<int>((lll_parameters.eta - 0.5) * 20);
  if (em < 0)
  {
    em = 0;
  }
  if (em > 9)
  {
    em = 9;
  }

  const double p = std::max(1.0, precision / 53.0);
  return kappa <
         p * eta_dep[static_cast<std::size_t>(em)] * dim_double_max[static_cast<std::size_t>(dm)];
}

/**
 * main function. Method determines whether heuristic, fast or proved
 */
template <class Z, class F>
int Wrapper::call_lll(ZZ_mat<Z> &bz, ZZ_mat<Z> &uz, ZZ_mat<Z> &u_inv_z, LLLMethod method,
                      int precision, double delta, double eta)
{
  using ZT = Z_NR<Z>;
  using FT = FP_NR<F>;

  if (lll_parameters.verbose)
  {
    log_stream() << "====== Wrapper: calling " << to_string(method) << "<" << num_type_str<Z>()
                 << "," << num_type_str<F>() << "> method";
    if (precision > 0)
    {
      log_stream() << " (precision=" << precision << ")";
    }
    log_stream() << " ======" << '\n';
  }

  report.float_type = float_type_of<F>();
  report.precision  = float_precision<F>(precision);
  last_call_proved  = (method == LLLMethod::proved);

  GSOOptions gso_options;
  gso_options.precision = precision;
  if (method == LLLMethod::proved)
  {
    gso_options.int_gram = true;
  }
  if (method == LLLMethod::fast)
  {
    gso_options.row_expo = true;
  }
  // The integer Gram path is exact; row-exponent scaling is mutually exclusive
  // with it and must not be enabled for the proved method.
  if (std::is_same_v<F, double> && method != LLLMethod::proved)
  {
    gso_options.row_expo = true;
  }
  if (method != LLLMethod::proved && precision == 0)
  {
    gso_options.force_long = true;
  }

  MatGSO<ZT, FT> m_gso(bz, uz, u_inv_z, gso_options);
  LLLReduction<ZT, FT> lll_obj(m_gso,
                               LLLParameters{.delta           = delta,
                                             .eta             = eta,
                                             .verbose         = lll_parameters.verbose,
                                             .early_reduction = lll_parameters.early_reduction,
                                             .siegel          = lll_parameters.siegel});
  lll_obj.last_early_red = last_early_red;
  lll_obj.lll();
  status         = lll_obj.status;
  last_early_red = std::max(last_early_red, lll_obj.last_early_red);

  if (lll_parameters.verbose)
  {
    log_stream() << "====== Wrapper: end of " << to_string(method) << " method ======\n" << '\n';
  }

  if (lll_obj.status == RedStatus::success)
  {
    return 0;
  }
  else if (lll_obj.status == RedStatus::gso_failure || lll_obj.status == RedStatus::babai_failure)
  {
    return lll_obj.final_kappa;
  }
  else
  {
    return -1;
  }
}

/**
 * pass the method to call_lll()
 */
template <class F> int Wrapper::fast_lll(double delta, double eta)
{
  return call_lll<mpz_t, F>(b, u, u_inv, LLLMethod::fast, 0, delta, eta);
}

template <class Z, class F>
int Wrapper::heuristic_lll(ZZ_mat<Z> &bz, ZZ_mat<Z> &uz, ZZ_mat<Z> &u_inv_z, int precision,
                           double delta, double eta)
{
  return call_lll<Z, F>(bz, uz, u_inv_z, LLLMethod::heuristic, precision, delta, eta);
}

template <class Z, class F>
int Wrapper::proved_lll(ZZ_mat<Z> &bz, ZZ_mat<Z> &uz, ZZ_mat<Z> &u_inv_z, int precision,
                        double delta, double eta)
{
  return call_lll<Z, F>(bz, uz, u_inv_z, LLLMethod::proved, precision, delta, eta);
}

/**
 * In heuristic_loop(), we only use double or mpfr_t.
 */
int Wrapper::heuristic_loop(int precision)
{
  int kappa;

  if (precision > std::numeric_limits<double>::digits)
  {
    kappa = heuristic_lll<mpz_t, mpfr_t>(b, u, u_inv, precision, lll_parameters.delta,
                                         lll_parameters.eta);
  }
  else
  {
    kappa = heuristic_lll<mpz_t, double>(b, u, u_inv, 0, lll_parameters.delta, lll_parameters.eta);
  }

  if (kappa == 0)
  {
    return 0;  // Success
  }
  else if (precision < good_prec && !little(kappa, precision))
  {
    return heuristic_loop(increase_prec(precision));
  }
  else
  {
    return proved_loop(precision);
  }
}

int Wrapper::proved_loop(int precision)
{
  int kappa;
#ifdef FPLLL_WITH_QD
  if (precision > PREC_DD || max_exponent * 2 > MAX_EXP_DOUBLE)
  {
    kappa =
        proved_lll<mpz_t, mpfr_t>(b, u, u_inv, precision, lll_parameters.delta, lll_parameters.eta);
  }
  else if (precision > std::numeric_limits<double>::digits)
  {
    kappa = proved_lll<mpz_t, dd_real>(b, u, u_inv, precision, lll_parameters.delta,
                                       lll_parameters.eta);
  }
  else
  {
    kappa = proved_lll<mpz_t, double>(b, u, u_inv, 0, lll_parameters.delta, lll_parameters.eta);
  }
#else
  if (precision > std::numeric_limits<double>::digits || max_exponent * 2 > MAX_EXP_DOUBLE)
  {
    kappa =
        proved_lll<mpz_t, mpfr_t>(b, u, u_inv, precision, lll_parameters.delta, lll_parameters.eta);
  }
  else
  {
    kappa = proved_lll<mpz_t, double>(b, u, u_inv, 0, lll_parameters.delta, lll_parameters.eta);
  }
#endif

  if (kappa == 0)
  {
    return 0;  // Success
  }
  else if (precision < good_prec)
  {
    return proved_loop(increase_prec(precision));
  }
  else
  {
    return -1;  // This point should never be reached
  }
}

/**
 * last call to LLL. Need to be proved_lll.
 */
int Wrapper::last_lll()
{

  /* <long, FT> */
  if (use_long)
  {
    ZZ_mat<long> empty_mat;
    int kappa;
    if (good_prec <= std::numeric_limits<double>::digits)
    {
      kappa = proved_lll<long, double>(b_long, empty_mat, empty_mat, good_prec,
                                       lll_parameters.delta, lll_parameters.eta);
    }
#ifdef FPLLL_WITH_QD
    else if (good_prec <= PREC_DD)
    {
      kappa = proved_lll<long, dd_real>(b_long, empty_mat, empty_mat, good_prec,
                                        lll_parameters.delta, lll_parameters.eta);
    }
#endif
    else
    {
      kappa = proved_lll<long, mpfr_t>(b_long, empty_mat, empty_mat, good_prec,
                                       lll_parameters.delta, lll_parameters.eta);
    }
    return kappa;
  }

  /* <mpfr, FT> */
  if (good_prec <= std::numeric_limits<double>::digits)
  {
    return proved_lll<mpz_t, double>(b, u, u_inv, good_prec, lll_parameters.delta,
                                     lll_parameters.eta);
  }
#ifdef FPLLL_WITH_QD
  else if (good_prec <= PREC_DD)
  {
    max_exponent =
        b.get_max_exp() + static_cast<int>(std::ceil(0.5 * std::log2(static_cast<double>(d) * n)));
    if (max_exponent * 2 < MAX_EXP_DOUBLE)
    {
      return proved_lll<mpz_t, dd_real>(b, u, u_inv, good_prec, lll_parameters.delta,
                                        lll_parameters.eta);
    }
  }
#endif
  return proved_lll<mpz_t, mpfr_t>(b, u, u_inv, good_prec, lll_parameters.delta,
                                   lll_parameters.eta);
}

/**
 * Wrapper.lll() calls
 *  - heuristic_lll()
 *  - fast_lll()
 *  - proved_lll()
 */
bool Wrapper::lll()
{
  if (b.get_rows() == 0 || b.get_cols() == 0)
  {
    report            = ReductionReport{};
    report.guaranteed = true;
    return true;
  }

  const bool heuristic_with_long =
      max_exponent < std::numeric_limits<long>::digits - 2 && u.empty() && u_inv.empty();
  const bool proved_with_long =
      2 * max_exponent < std::numeric_limits<long>::digits - 2 && u.empty() && u_inv.empty();

  int kappa = 0;

  /* small matrix */
  if (heuristic_with_long)
  {
    set_use_long(true);
    /* try heuristic_lll <long, double> */
    ZZ_mat<long> empty_mat;
    heuristic_lll<long, double>(b_long, empty_mat, empty_mat, 0, lll_parameters.delta,
                                lll_parameters.eta);
  }
  /* large matrix */
  else
  {
    /* try fast_lll<mpz_t, double> */
    kappa            = fast_lll<double>(lll_parameters.delta, lll_parameters.eta);
    bool lll_failure = (kappa != 0);
    int last_prec;

    /* try fast_lll<mpz_t, dd_real> */
#ifdef FPLLL_WITH_QD
    if (lll_failure)
    {
      kappa       = fast_lll<dd_real>(lll_parameters.delta, lll_parameters.eta);
      lll_failure = kappa != 0;
    }
    last_prec = PREC_DD;
#else
    last_prec = std::numeric_limits<double>::digits;
#endif

    /* loop */
    if (lll_failure)
    {
      const int prec_d = std::numeric_limits<double>::digits;
      if (little(kappa, last_prec))
      {
        kappa = proved_loop(prec_d);
      }
      else
      {
        kappa = heuristic_loop(increase_prec(prec_d));
      }
    }
  }

  set_use_long(proved_with_long);
  /* final LLL */
  kappa = last_lll();
  set_use_long(false);
  report.status     = status;
  report.guaranteed = kappa == 0 && last_call_proved && report.precision >= good_prec;
  return kappa == 0;
}

/**
 * set b_long <-- b or b <-- b_long
 */
void Wrapper::set_use_long(bool value)
{
  if (!use_long && value)
  {
    if (b_long.empty())
    {
      b_long.resize(d, n);
    }
    for (int i = 0; i < d; i++)
    {
      for (int j = 0; j < n; j++)
      {
        b_long(i, j) = b(i, j).get_si();
      }
    }
  }
  else if (use_long && !value)
  {
    for (int i = 0; i < d; i++)
    {
      for (int j = 0; j < n; j++)
      {
        b(i, j) = b_long(i, j).get_si();
      }
    }
  }
  use_long = value;
}

int Wrapper::increase_prec(int precision) { return std::min(precision * 2, good_prec); }

/* Set of methods for the HLLL wrapper. */

/**
 * main function to call hlll
 * Return true if success, false otherwise
 */
template <class F> bool Wrapper::call_hlll(LLLMethod method, int precision)
{
  using FT = FP_NR<F>;

  if (hlll_parameters.verbose)
  {
    log_stream() << "====== Wrapper: calling " << to_string(method) << "<mpz_t,"
                 << num_type_str<F>() << "> method";
    if (precision > 0)
    {
      log_stream() << " (precision=" << precision << ")";
    }
    log_stream() << " ======" << '\n';
  }

  report.float_type = float_type_of<F>();
  report.precision  = float_precision<F>(precision);
  last_call_proved  = (method == LLLMethod::proved);

  HouseholderOptions householder_options;
  householder_options.precision = precision;
  if (method == LLLMethod::fast)
  {
    householder_options.row_expo   = true;
    householder_options.force_long = true;
  }

  MatHouseholder<Z_NR<mpz_t>, FT> m(b, u, u_inv, householder_options);
  HLLLReduction<Z_NR<mpz_t>, FT> hlll_obj(m, hlll_parameters);
  hlll_obj.hlll();
  status = hlll_obj.get_status();

  if (hlll_parameters.verbose)
  {
    log_stream() << "====== Wrapper: end of " << to_string(method) << " method ======\n" << '\n';
  }

  return status == RedStatus::success;
}

/**
 * last call to HLLL. Needs to be proved_hlll.
 */
bool Wrapper::last_hlll()
{
  /* <mpfr, FT> */
  return proved_hlll<mpfr_t>(good_prec);
}

int Wrapper::hlll_proved_loop(int precision)
{
  const bool complete = proved_hlll<mpfr_t>(precision);

  if (complete)
  {
    return 0;  // Success
  }
  else if (precision < good_prec)
  {
    return hlll_proved_loop(increase_prec(precision));
  }
  else
  {
    return -1;  // This point should never be reached
  }
}

/**
 * pass the method to call_hlll()
 */
template <class F> bool Wrapper::fast_hlll() { return call_hlll<F>(LLLMethod::fast, 0); }

template <class F> bool Wrapper::proved_hlll(int precision)
{
  return call_hlll<F>(LLLMethod::proved, precision);
}

bool Wrapper::hlll()
{
  if (b.get_rows() == 0 || b.get_cols() == 0)
  {
    report            = ReductionReport{};
    report.guaranteed = true;
    return true;
  }

  int last_prec      = std::numeric_limits<double>::digits;
  bool hlll_complete = false;

  // Take the fast HLLL path first; hplll-style shortcuts can be revisited
  // separately.
  hlll_complete = fast_hlll<double>();

  /* try fast_hlll<mpz_t, dd_real> */
#ifdef FPLLL_WITH_QD
  if (!hlll_complete)
  {
    hlll_complete = fast_hlll<dd_real>();
    last_prec     = PREC_DD;
  }
#endif  // FPLLL_WITH_QD

  /* loop */
  if (!hlll_complete)
  {
    (void)hlll_proved_loop(last_prec);
  }

  // The final call must be proved.
  const bool complete = last_hlll();
  report.status       = status;
  report.guaranteed   = complete && last_call_proved && report.precision >= good_prec;
  return complete;
}

}  // namespace fplll
