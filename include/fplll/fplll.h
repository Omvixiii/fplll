// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_H
#define FPLLL_H

/* fplll is a C++23 library: the public headers use std::format, enum class
   and designated initializers, so the language mode and the standard library
   must both implement C++23.  MSVC reports the language version through
   _MSVC_LANG.  Meson already rejects a compiler that cannot build in
   C++23 mode for in-tree builds; this guard additionally covers downstream
   consumers that compile the installed headers directly. */
#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) < 202302L
#error fplll requires a C++23 compiler and standard library
#endif

#include "defs.h"                 // IWYU pragma: export
#include "enum/enumerate.h"       // IWYU pragma: export
#include "gso/gso.h"              // IWYU pragma: export
#include "gso/gso_gram.h"         // IWYU pragma: export
#include "gso/householder.h"      // IWYU pragma: export
#include "pruner/pruner.h"        // IWYU pragma: export
#include "reduction/bkz.h"        // IWYU pragma: export
#include "reduction/bkz_param.h"  // IWYU pragma: export
#include "reduction/hlll.h"       // IWYU pragma: export
#include "reduction/lll.h"        // IWYU pragma: export
#include "svpcvp.h"               // IWYU pragma: export
#include "threadpool.h"           // IWYU pragma: export
#include "util.h"                 // IWYU pragma: export

#endif
