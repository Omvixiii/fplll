# Using fplll

## How to use

The build places `fplll` and `latticegen` under `build/tools/` (for example,
`build/tools/fplll`). Installation places them in the prefix's `bin/`
directory. `llldiff` is a test helper and is not installed.
The examples below assume the command-line tools are on the search path (for
example after `meson install` or by prefixing the build path), so the programs
can be invoked as `fplll` and `latticegen`.

### latticegen

`latticegen` is a utility for generating matrices (rows form input
lattice basis vectors).

The options are:

* `r` `d` `b` : generates a knapsack like matrix of dimension d x (d+1) and b bits (see, e.g., [[S09](#s09)]): the i-th vector starts with a random integer of bit-length <=b and the rest is the i-th canonical unit vector.
* `s` `d` `b` `b2` : generates a d x d matrix of a form similar to that is involved when trying to find rational approximations to reals with the same small denominator (see, e.g., [[LLL82](#lll82)]): the first vector starts with a random integer of bit-length <=b2 and continues with d-1 independent integers of bit-lengths <=b; the i-th vector for i>1 is the i-th canonical unit vector scaled by a factor 2^b.
* `u` `d` `b` : generates a d x d matrix whose entries are independent integers of bit-lengths <=b.
* `n` `d` `b` `c` : generates an ntru-like matrix. If char is 'b', then it first samples an integer q of bit-length b, whereas if char is 'q', then it sets q to the provided value. Then it samples a uniform h in the ring Z_q[x]/(x^n-1). It finally returns the 2 x 2 block matrix [[I, Rot(h)], [0, q*I]], where each block is d x d, the first row of Rot(h) is the coefficient vector of h, and the i-th row of Rot(h) is the shift of the (i-1)-th (with last entry put back in first position), for all i>1. Warning: this does not produce a genuine ntru lattice with h a genuine public key (see [[HPS98](#hps98)]).
* `N` `d` `b` `c` : as the previous option, except that the constructed matrix is [[q*I, 0], [Rot(h), I]].
* `q` `d` `k` `b` `c` : generates a q-ary matrix. If char is 'b', then it first samples an integer q of bit-length b; if char is 'p', it does the same and updates q to the smallest (probabilistic) prime that is greater; if char is 'q', then it sets q to the provided value. It returns a 2 x 2 block matrix [[I, H], [0, q*I]], where H is (d-k) x k and uniformly random modulo q. These bases correspond to the SIS/LWE q-ary lattices (see [[MR09](#mr09)]). Goldstein-Mayer lattices correspond to k=1 and q prime (see [[GM03](#gm03)]).
* `t` `d` `f` : generates a d x d lower-triangular matrix B with B_ii = 2^(d-i+1)^f for all i, and B_ij is uniform between -B_jj/2 and B_jj/2 for all j<i.
* `T` `d` : also takes as input a d-dimensional vector vec read from a file. It generates a d x d lower-triangular matrix B with B_ii = vec[i] for all i and B_ij is uniform between -B_jj/2 and B_jj/2 for all j<i.

The generated matrix is printed in stdout.

Note that by default, the random bits always use the same seed, to ensure reproducibility. The seed may be changed with the option `-randseed <integer>` or by using the current time (in seconds) `-randseed time`. If you use this option, it must be the first one on the command line.

### fplll command

`fplll` does LLL, BKZ, HKZ or SVP on a matrix (considered as a set of row
vectors) given in stdin or in a file as parameter.

The options are:

* `-a lll` : LLL-reduction (default).
* `-a bkz` : BKZ-reduction.
* `-a hkz` : HKZ-reduction.
* `-a svp` : prints a shortest non-zero vector of the lattice.
* `-a sdb` : self dual variant of BKZ-reduction.
* `-a sld` : slide reduction.
* `-a cvp` : prints the vector in the lattice closest to the input vector.
* `-v` : verbose mode.
* `-nolll` : does not apply to LLL-reduction. In the case of bkz, hkz and svp, by default, the input basis is LLL-reduced before anything else. This option allows to remove that initial LLL-reduction (note that other calls to LLL-reduction may occur during the execution). In the case of hlll, verify if the input basis is HLLL-reduced.

* `-a hlll` : HLLL-reduction.

Options for LLL-reduction:

* `-d delta` :     δ (default=0.99)
* `-e eta` :       η (default=0.51). See [[NS09](#ns09)] for the definition of (δ,η)-LLL-reduced bases.
* `-l lovasz` :    if !=0 Lovasz's condition. Otherwise, Siegel's condition (default: Lovasz). See [[A02](#a02)] for the definition of Siegel condition.

* `-f mpfr` : sets the floating-point type to MPFR (default if `m=proved`).
* `-p precision` : precision of the floating-point arithmetic, works only with `-f mpfr`.
* `-f dd` : sets the floating-point type to double-double.
* `-f qd` : sets the floating-point type to quad-double.
* `-f double` : sets the floating-point type to double (default if `m=fast`).
* `-f longdouble` : sets the floating-point type to long double.

* `-z mpz` : sets the integer type to mpz, the integer type of GMP (default).
* `-z int` : sets the integer type to int.
* `-z long` : as `-z int`.

* `-m wrapper` : uses the wrapper. (default if `z=mpz`).
* `-m fast` : uses the fast method, works only with `-f double`.
* `-m heuristic` : uses the heuristic method.
* `-m proved` : uses the proved version of the algorithm.
* `-y` : early reduction.

With the wrapper or the proved version, it is guaranteed that the basis is LLL-reduced with δ'=2×δ-1
and η'=2×η-1/2. For instance, with the default options, it is guaranteed that the basis is
(0.98,0.52)-LLL-reduced.

Options for BKZ-reduction:

* `-b block_size` :            block size, mandatory, between 2 and the number of vectors.

* `-f float_type` :            same as LLL (`-p` is required if `float_type=mpfr`).
* `-p precision` :             precision of the floating-point arithmetic with `-f mpfr`.

* `-bkzmaxloops loops` :       maximum number of full loop iterations.
* `-bkzmaxtime time` :         stops after `time` seconds (up to completion of the current loop iteration).
* `-bkzautoabort` :            stops when the average slope of the log ||b_i*||'s does not decrease fast enough.

Without any of the last three options, BKZ runs until no block has been updated for a full loop iteration.

* `-s filename.json` :         use strategies for preprocessing and pruning parameter (`strategies/default.json` in the source tree; installed under `share/strategies/`). Experimental.

* `-bkzghbound factor` :       multiplies the Gaussian heuristic by `factor` (of float type) to set the enumeration radius of the SVP calls.
* `-bkzboundedlll` : restricts the LLL call before considering a block to vector indices within that block.

* `-bkzdumpgso file_name` :     dumps the log ||b_i*|| 's in specified file.

Output formats:

* `-of` : prints new line (if `-a [lll|bkz]`)
* `-of b` : prints the basis (if `-a [lll|bkz]`, this value by default)
* `-of bk` : prints the basis (if `-a [lll|bkz]`, format compatible with sage)
* `-of c` : prints the closest vector (if `-a cvp`, this value by default)
* `-of s` : prints the closest vector (if `-a svp`, this value by default)
* `-of t` : prints status (if `-a [lll|bkz|cvp|svp]`)
* `-of u` : prints unimodular matrix (if `-a [lll|bkz]`)
* `-of uk` : prints unimodular matrix (if `-a [lll|bkz]`, format compatible with sage)
* `-of v` : prints inverse of u (if `-a lll`)
* `-of vk` : prints inverse of u (if `-a lll`, format compatible with sage)

A combination of these option is allowed (e.g., `-of bkut`).

Only for `-a hlll`:

* `-t theta` : θ (default=0.001). See [[MSV09](#msv09)] for the definition of (δ,η,θ)-HLLL-reduced bases.
* `-c c` : arbitrary positive constant for HLLL, used as the size-reduction threshold (default=0.1).

### llldiff

`llldiff` compares two bases (b1,...,bd) and (c1,...c_d'): they are considered
equal iff d=d' and for any i, bi = +- ci. Concretely, if basis B is in file 'B.txt' and if basis C is in file 'C.txt' (in the fplll format), then one may run `cat B.txt C.txt | build/tests/llldiff` (`llldiff` is built but not installed).

### How to use as a library

See [API documentation](https://fplll.github.io/fplll/annotated.html) and [tests](https://github.com/fplll/fplll/tree/master/tests) as a source of examples.

### Multicore support

This library does not currently use multiple cores and running multiple threads working on the same object `IntegerMatrix`, `LLLReduction`, `MatGSO` etc. is not supported.  Running multiple threads working on *different* objects, however, is supported. That is, there are no task-specific global variables and it is safe to e.g. reduce several lattices in parallel in the same process.  Note that some process-wide state is shared: the GMP random state used by BKZ rerandomization and the matrix generators (mutex-protected, so calls are safe but their interleaving determines the random stream), the MPFR default precision (thread-local only if MPFR was built thread-safe), and the log sink set with `fplll::set_log_stream` (the installed stream must outlive its use and concurrent writes must be synchronized by the caller).

As an exception to the above, fplll has an implementation of parallel lattice point enumeration.
To enable this implementation, make sure you compile with the maximum parallel enumeration dimension greater than 0. Note that increasing this value will increase the compile-time due to the use of templates.

At present fplll does not contain strategies for multi-core pruned enumeration, and so speedups for pruned enumeration may be sub-linear (see [this](https://martinralbrecht.wordpress.com/2020/10/26/multicore-bkz-in-fplll/) for more information). On the other hand, unpruned enumeration appears to scale linearly.

## Examples

LLL reduction on a generated lattice:

```sh
latticegen r 10 1000 | fplll
```

For file input, put the following matrix in a file named `matrix`:

```text
[[10 11]
 [11 12]]
```

Then `fplll matrix` produces a reduced basis:

```text
[[0 1]
 [1 0]
]
```

Use `-randseed` to choose a deterministic seed or the current time:

```sh
latticegen -randseed 1234 r 10 1000 | fplll
latticegen -randseed time u 10 16 | fplll
```

Solve SVP or CVP:

```sh
latticegen r 30 3000 | fplll -a svp
printf '%s\n' '[[17 42 4][50 75 108][11 47 33]][100 101 102]' | fplll -a cvp
```

## Alternative interfaces

* [fpylll](https://github.com/malb/fpylll) is a stand-alone Python interface for fplll.
* fplll is included in [Sage](http://sagemath.org), see documentation for [IntegerMatrix](http://doc.sagemath.org/html/en/reference/matrices/sage/matrix/matrix_integer_dense.html) and [IntegerLattice](http://doc.sagemath.org/html/en/reference/modules/sage/modules/free_module_integer.html).

## Bibliography

### A02

A. Akhavi. Random lattices, threshold phenomena and efficient reduction algorithms. Theor. Comput. Sci. 287(2): 359-385 (2002)

### Chen13

Y. Chen, Lattice reduction and concrete security of fully homomorphic encryption.

### CN11

Y. Chen and P. Q. Nguyen. BKZ 2.0: Better Lattice Security Estimates. ASIACRYPT 2011: 1-20

### GM03

D. Goldstein and A. Mayer. On the equidistribution of Hecke points. Forum Mathematicum, 15:165–189 (2003)

### GN08

N. Gama and P. Q. Nguyen. Finding Short Lattice Vectors within Mordell's Inequality. STOC 2008: 207-216

### GNR13

N. Gama, P. Q. Nguyen and Oded Regev. Lattice Enumeration Using Extreme Pruning.

### HPS98

J. Hoffstein, J. Pipher, J. H. Silverman. NTRU: A Ring-Based Public Key Cryptosystem. ANTS 1998: 267-288

### K83

R. Kannan. Improved algorithms for integer programming and related lattice problems. STOC 1983, 99-108

### FP85

U. Fincke and M. Pohst. Improved methods for calculating vectors of short length in a lattice, including a complexity analysis. Math. Comp., 44(170):463–471 (1985)

### LLL82

A. K. Lenstra, H. W. Lenstra, Jr. and L. Lovasz. Factoring polynomials with rational coefficients. Math. Ann., 261: 515–534 (1982)

### MSV09

I. Morel, D. Stehle and G. Villard. H-LLL: using Householder inside LLL. ISSAC 2009: 271-278

### MW16

D. Micciancio and M. Walter. Practical, Predictable Lattice Basis Reduction. EUROCRYPT 2016: 820-849

### MR09

D. Micciancio and O. Regev. Post-Quantum Cryptography. Chapter of Lattice-based Cryptography, 147-191 (2009)

### NS09

P. Q. Nguyen and D. Stehle. An LLL Algorithm with Quadratic Complexity. SIAM J. Comput. 39(3): 874-903 (2009)

### S09

D. Stehle. Floating-Point LLL: Theoretical and Practical Aspects. The LLL Algorithm 2009: 179-213

### SE94

C.-P. Schnorr and M. Euchner. Lattice basis reduction: Improved practical algorithms and solving subset sum problems. Math. Program. 66: 181-199 (1994)

## Credit

### Historical maintainers

Earlier project documentation listed:

* Martin Albrecht, <martinralbrecht@googlemail.com>
* Shi Bai, <shih.bai@gmail.com>

### Contributors

The following people have contributed to fplll:

* Martin Albrecht
* Shi Bai
* Guillaume Bonnoron
* David Cade
* Léo Ducas
* Joop van de Pol
* Xavier Pujol
* Damien Stehlé
* Marc Stevens
* Gilles Villard
* Michael Walter

Please add yourself here if you make a contribution.

### Acknowledgments


* David H. Bailey for `QD`.

* Sylvain Chevillard, Christoph Lauter and Gilles Villard for the `configure/make/make install` packaging.

* Timothy Abbott, Michael Abshoff, Bill Allombert, John Cannon, Sylvain Chevillard, Julien Clement, Andreas Enge, Jean-Pierre Flori, Laurent Fousse, Guillaume Hanrot, Jens Hermans, Jerry James, Christoph Lauter, Tancrède Lepoint, Andrew Novocin, Willem Jan Palenstijn, Patrick Pelissier, Julien Puydt, Michael Schneider, Thiemo Seufer, Allan Steel, Gilles Villard and Paul Zimmermann for their support and for many suggestions that helped debugging and improving this code.

* [CONTRIBUTING.md](../CONTRIBUTING.md) is taken, almost verbatim, from [the djangopackages contributing guide](https://github.com/pydanny/djangopackages/blob/master/docs/contributing.rst)

* JSON parsing uses [nlohmann/json](https://github.com/nlohmann/json) as an
  external dependency resolved by Meson (a system package when available,
  otherwise the pinned WrapDB wrap), not a bundled source file.

* This project has been supported by ERC Starting Grant ERC-2013-StG-335086-LATTAC, by the European Union PROMETHEUS project (Horizon 2020 Research and Innovation Program, grant 780701), by EPSRC grant EP/P009417/1 and by EPSRC grant EP/S020330/1.

### Library diagnostics

The library discards diagnostic output by default.  `fplll::set_log_stream`
redirects it to an `std::ostream` (for example `std::cerr`), and
`fplll::set_log_stream(nullptr)` restores the discarding default.  The command
line tools install `std::cerr` when verbose mode is enabled (and for the
pruning-parameter action); verbose algorithm flags then write through the same
sink.
