/* Copyright (C) 2005-2008 Damien Stehle.
   Copyright (C) 2007 David Cade.
   Copyright (C) 2011 Xavier Pujol.

   This file is part of fplll. fplll is free software: you
   can redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License as published by the Free Software Foundation,
   either version 2.1 of the License, or (at your option) any later version.

   fplll is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with fplll. If not, see <http://www.gnu.org/licenses/>. */

#ifndef FPLLL_H
#define FPLLL_H

#if defined(_MSVC_LANG)
#if _MSVC_LANG < 201103L
#error fplll needs at least a C++11 compliant compiler
#endif
#elif __cplusplus < 201103L
#error fplll needs at least a C++11 compliant compiler
#endif

#include "bkz.h"            // IWYU pragma: export
#include "bkz_param.h"      // IWYU pragma: export
#include "gso_gram.h"       // IWYU pragma: export
#include "hlll.h"           // IWYU pragma: export
#include "pruner/pruner.h"  // IWYU pragma: export
#include "svpcvp.h"         // IWYU pragma: export
#include "threadpool.h"     // IWYU pragma: export
#include "util.h"           // IWYU pragma: export
#include "wrapper.h"        // IWYU pragma: export

#endif
