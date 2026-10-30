// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detail/float_type.h"
#include <fplll/defs.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/util.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <gmp.h>
#include <numbers>
#include <ostream>
#include <streambuf>

namespace fplll
{

namespace
{

class NullLogBuffer : public std::streambuf
{
protected:
  int overflow(int c) override { return c; }
};

// Constructed lazily: a namespace-scope std::ostream may throw while static
// initialization runs, which would terminate the program before main().
std::ostream &null_log_stream()
{
  static NullLogBuffer null_log_buffer;
  static std::ostream stream(&null_log_buffer);
  return stream;
}

std::atomic<std::ostream *> current_log_stream{nullptr};

}  // namespace

std::ostream &log_stream()
{
  std::ostream *stream = current_log_stream.load(std::memory_order_acquire);
  return stream != nullptr ? *stream : null_log_stream();
}

void set_log_stream(std::ostream *stream)
{
  current_log_stream.store(stream, std::memory_order_release);
}

namespace
{

enum class MinPrecAlgo : std::uint8_t
{
  gso,
  l2,
};

int compute_min_prec(double &rho, int d, double delta, double eta, double epsilon, MinPrecAlgo algo)
{
  ScopedPrecision precision_guard(53);
  FP_NR<mpfr_t> f_minprec, f_rho, f_d, f_eta, f_delta, f_epsilon, tmp1, tmp2;

  // These four conversions are exact
  f_d       = static_cast<double>(d);
  f_eta     = eta;
  f_delta   = delta;
  f_epsilon = epsilon;
  if (algo == MinPrecAlgo::l2)
  {
    // eta - 0.5 is an exact fp operation
    if (f_epsilon > eta - 0.5)
    {
      f_epsilon = eta - 0.5;
    }
    tmp1 = 1.0;
    tmp1.sub(tmp1, f_delta, GMP_RNDD);
    if (f_epsilon > tmp1)
    {
      f_epsilon = tmp1;
    }
    // now fEpsilon <= min(epsilon, eta - 0.5, 1 - delta);
  }
  // Computes tmp1 >= (1 + eta) ^ 2 + epsilon
  tmp1 = 1.0;                       // exact
  tmp1.add(f_eta, tmp1, GMP_RNDU);  // >= 1 + eta
  tmp1.mul(tmp1, tmp1, GMP_RNDU);   // >= (1 + eta) ^ 2
  tmp1.add(tmp1, f_epsilon, GMP_RNDU);
  // Computes tmp2 <= delta - eta ^ 2
  tmp2.mul(f_eta, f_eta, GMP_RNDU);
  tmp2.sub(f_delta, tmp2, GMP_RNDD);
  FPLLL_CHECK(tmp2 > 0, "invalid LLL parameters, eta must be < sqrt(delta)");
  // Computes rho >= ((1 + eta) ^ 2 + epsilon) / (delta - eta ^ 2)
  f_rho.div(tmp1, tmp2, GMP_RNDU);
  rho = f_rho.get_d(GMP_RNDU);

  /* Computes minprec >= constant + 2 * log2(d) - log2(epsilon) + d * log2(rho)
     (constant = 5 for GSO, 10 for LLL) */
  tmp1.log(f_d, GMP_RNDU);         // >= log(d)
  tmp1.mul_2si(tmp1, 1);           // >= 2 * log(d)
  tmp2.log(f_epsilon, GMP_RNDD);   // <= log(epsilon) (<= 0)
  tmp1.sub(tmp1, tmp2, GMP_RNDU);  // >= 2 * log(d) - log(epsilon)
  tmp2.log(f_rho, GMP_RNDU);       // >= log(rho)
  tmp2.mul(f_d, tmp2, GMP_RNDU);   // >= d * log(rho)
  tmp1.add(tmp1, tmp2, GMP_RNDU);  // >= 2*log(d)-log(epsilon)+d*log(rho)
  tmp2 = 2.0;                      // exact
  tmp2.log(tmp2, GMP_RNDD);        // <= log(2)
  tmp1.div(tmp1, tmp2, GMP_RNDU);  // >= 2*log2(d)-log2(epsilon)+d*log2(rho)
  tmp2 = (algo == MinPrecAlgo::l2) ? 10.0 : 5.0;
  f_minprec.add(tmp1, tmp2, GMP_RNDU);
  const int minprec = static_cast<int>(std::ceil(f_minprec.get_d(GMP_RNDU)));
  mpfr_free_cache();
  return minprec;
}

}  // namespace

/* State of the random generator (declared in nr.h, must be defined in exactly
   one source file) */
gmp_randstate_t RandGen::gmp_state;

int gso_min_prec(double &rho, int d, double delta, double eta, double epsilon)
{
  return compute_min_prec(rho, d, delta, eta, epsilon, MinPrecAlgo::gso);
}

int l2_min_prec(int d, double delta, double eta, double epsilon)
{
  double rho;
  return compute_min_prec(rho, d, delta, eta, epsilon, MinPrecAlgo::l2);
}

int hlll_min_prec(int d_i, int n_i, double delta_d, double eta_d, double theta_d, double c_d)
{
  FPLLL_CHECK(delta_d < 1.0 && delta_d >= 0.25, "delta must be in [1/4, 1).");
  FPLLL_CHECK(theta_d >= 0.0, "theta must be positive.");
  FPLLL_CHECK(eta_d >= 0.5, "theta must be larger than or equal to 0.5.");
  FPLLL_CHECK(eta_d - theta_d > 0.5, "eta - theta must be larger than 0.5.");

  ScopedPrecision precision_guard(53);
  FP_NR<mpfr_t> d, n, delta, eta, theta, c, alpha, c0, c1, rho, phi, p0, p;
  FP_NR<mpfr_t> ftmp0, ftmp1, ftmp2, ftmp3, ftmp4;

  d     = d_i;
  n     = n_i;
  delta = delta_d;
  eta   = eta_d;
  theta = theta_d;
  c     = c_d;

  // ftmp0 = (1 + theta^2) * delta - eta^2
  ftmp0 = (1.0 + theta * theta) * delta - eta * eta;
  // ftmp0 = sqrt((1 + theta^2) * delta - eta^2)
  ftmp0.sqrt(ftmp0);

  // alpha = theta * eta + sqrt((1 + theta^2) * delta - eta^2) / (delta - eta^2)
  alpha = (theta * eta + ftmp0) / (delta - eta * eta);

  // ftmp0 = 3 / 2
  ftmp0 = 3.0 / 2.0;
  // ftmp0 = sqrt(3 / 2)
  ftmp0.sqrt(ftmp0);
  // ftmp1 = 1 - eta - theta
  ftmp1 = 1.0 - eta - theta;
  // ftmp1 = |1 - eta - theta|
  ftmp1.abs(ftmp1);
  ftmp2 = 6.0;
  // ftmp2 = sqrt(6)
  ftmp2.sqrt(ftmp2);
  // ftmp3 = 1 + d * eta^2
  ftmp3 = 1.0 + d * eta * eta;
  // ftmp3 = sqrt(1 + d * eta^2)
  ftmp3.sqrt(ftmp3);
  // ftmp4 = sqrt(d)
  ftmp4.sqrt(d);
  // ftmp0 = 1 + |1 - eta - theta| * alpha / ((eta + theta) * (-1 + sqrt(3/2)))
  ftmp0 = (1.0 + ftmp1 * alpha) / ((eta + theta) * (-1.0 + ftmp0));
  // ftmp1 = 4 * sqrt(6) / (1 + eta) * sqrt(1 + d * eta^2)
  ftmp1 = 4.0 * ftmp2 / (1.0 + eta) * ftmp3;
  // ftmp0 = max(1 + |1 - eta - theta| * alpha / ((eta + theta) * (-1 + sqrt(3/2))),
  //             4 * sqrt(6) / (1 + eta) * sqrt(1 + d * eta^2))
  if (ftmp1 > ftmp0)
  {
    ftmp0 = ftmp1;
  }
  // c0 = max(...) * n * sqrt(d)
  c0 = ftmp0 * n * ftmp4;

  // c1 = 8 * d * (n + 9) * c0
  c1 = 8.0 * d * (n + 9.0) * c0;

  // rho = (1 + eta + theta) * alpha
  rho = (1.0 + eta + theta) * alpha;

  // ftmp0 = rho^(d + 1) (since we want to compute phi(d))
  ftmp0.pow_si(rho, d_i + 1);
  // phi(d) = c1 * (1 + 1 / theta) * ftmp0
  phi = c1 * (1.0 + 1.0 / theta) * ftmp0;

  // ftmp0 = alpha^d
  ftmp0.pow_si(alpha, d_i);
  // ftmp0 = log(d^3 * phi(d) * alpha^d / theta)
  ftmp0.log(d * d * d * phi * ftmp0 / theta);
  // ftmp1 = log(2)
  ftmp1.log(2);
  // ftmp0 = log(d^3 * phi(d) * alpha^d / theta) / log(2)
  ftmp0 = ftmp0 / ftmp1;
  // p0 = log2(d^3 * phi(d) * alpha^d / theta) + 16 + c * d / 2
  p0 = ftmp0 + 16.0 + c * d / 2.0;

  // ftmp0 = log(1 - delta)
  ftmp0.log(1.0 - delta);
  // ftmp0 = log(1 - delta) / log(2)
  ftmp0 = ftmp0 / ftmp1;
  // ftmp2 = log(eta - theta - 1/2)
  ftmp2.log(eta - theta - 0.5);
  // ftmp2 = log(eta - theta - 1/2) / log(2)
  ftmp2 = ftmp2 / ftmp1;
  // p = p0 + 1 - log2(1 - delta) - log2(eta - theta - 1 / 2)
  p = p0 + 1.0 - ftmp0 - ftmp2;

  // Convert p in int
  return static_cast<int>(std::ceil(p.get_d(GMP_RNDU)));
}

/** Computes the volume of a d-dimensional hypersphere of radius 1. */
void sphere_volume(FP_NR<mpfr_t> &volume, int d)
{
  FP_NR<mpfr_t> rtmp1;
  const int half_d = d / 2;
  volume           = std::pow(std::numbers::pi, static_cast<double>(half_d));

  if (d % 2 == 0)
  {
    for (int i = 1; i <= d / 2; i++)
    {
      rtmp1 = static_cast<double>(i);
      volume.div(volume, rtmp1);
    }
  }
  else
  {
    for (int i = 0; i <= d / 2; i++)
    {
      rtmp1 = 2.0 / static_cast<double>(2 * i + 1);
      volume.mul(volume, rtmp1);
    }
  }
}

/** Estimates the cost of the enumeration for SVP. */
void cost_estimate(FP_NR<mpfr_t> &cost, const FP_NR<mpfr_t> &bound, const Matrix<FP_NR<mpfr_t>> &r,
                   int dimMax)
{
  FP_NR<mpfr_t> det, level_cost, tmp1;
  det  = 1.0;
  cost = 0.0;

  for (int i = dimMax - 1; i >= 0; i--)
  {
    tmp1.div(bound, r(i, i));
    det.mul(det, tmp1);

    level_cost.sqrt(det);
    sphere_volume(tmp1, dimMax - i);
    level_cost.mul(level_cost, tmp1);

    cost.add(cost, level_cost);
  }
}

}  // namespace fplll
