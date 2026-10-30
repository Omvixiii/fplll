// SPDX-FileCopyrightText: 2015-2017 Martin Albrecht, Leo Ducas
// SPDX-FileCopyrightText: 2018 Shi Bai
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruning_params.h
    Pruning parameters for one enumeration radius. */

#ifndef FPLLL_PRUNING_PARAMS_H
#define FPLLL_PRUNING_PARAMS_H

#include "../defs.h"

#include <vector>

namespace fplll
{

/** Pruning parameters for one radius (expressed as a ratio to the Gaussian
    heuristic). */
struct PruningParams
{
  double gh_factor = 1.0;            //< radius^2 / Gaussian heuristic^2
  std::vector<double> coefficients;  //< pruning coefficients
  double expectation  = 1.0;         //< expected success probability or number of solutions
  PrunerMetric metric = PrunerMetric::probability_of_shortest;  //< optimized objective
  std::vector<double> detailed_cost;                            //< expected nodes per level
};

}  // namespace fplll

#endif /* FPLLL_PRUNING_PARAMS_H */
