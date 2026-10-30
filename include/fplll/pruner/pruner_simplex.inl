// SPDX-FileCopyrightText: 2015-2017 Martin Albrecht, Leo Ducas
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner_simplex.inl
    Inline functions computing the volume of an even simplex (used by
    pruner_prob.inl and pruner_cost.inl). */

template <class FT> FT Pruner<FT>::eval_poly(const int ld, const poly &p, const FT &x)
{
  FT acc = 0.0;
  for (int i = ld; i >= 0; --i)
  {
    acc = acc * x;
    acc = acc + p[i];
  }
  return acc;
}

template <class FT> void Pruner<FT>::integrate_poly(const int ld, poly &p)
{
  for (int i = ld; i >= 0; --i)
  {
    FT tmp   = i + 1.;
    p[i + 1] = p[i] / tmp;
  }
  p[0] = 0.0;
}

/** Volume of an even simplex. */
template <class FT> FT Pruner<FT>::relative_volume(const int rd, const evec &b)
{
  poly P(rd + 1);
  P[0]   = 1;
  int ld = 0;
  for (int i = rd - 1; i >= 0; --i)
  {
    integrate_poly(ld, P);
    ld++;
    P[0] = -1.0 * eval_poly(ld, P, b[i] / b[rd - 1]);
  }
  FT res = P[0] * tabulated_factorial[rd];
  return (rd % 2) ? -res : res;
}
