# eflomal
Efficient Low-Memory Aligner

This work is a fork of Robert Ösling's [eflomal](https://github.com/robertostling/eflomal) with a few fixes and additional features:
* when builing on Mac OS, remove `-lrt` from `LDFLAGS`
* add `mkmodel.py` script for computing translation probabilities directly from a parallel corpus; this first computes alignment using `eflomal` then derives probabilities from it

`eflomal` is a word alignment tool based on
[efmaral](https://github.com/robertostling/efmaral), with the following main
differences:
 * More compact data structures are used, so memory requirements are much
   lower (by orders of magnitude).
 * The estimation of alignment variable marginals is done one sentence at a
   time, which also saves a lot of memory at no detectable cost in accuracy.

Technical details relevant to both `efmaral` and `eflomal` can be found in
the following article:
 * [Östling and Tiedemann (2016)](https://ufal.mff.cuni.cz/pbml/106/art-ostling-tiedemann.pdf) ([BibTeX](http://www.robos.org/sections/research/robert_bib.html#Ostling2016efmaral)).

## Installing

To compile and install the C binary and the Python bindings:

    make
    sudo make install
    python3 setup.py install

edit `Makefile` manually if you want to install somewhere other than the
default `/usr/local/bin`. Note that the `align.py` script now uses the
`eflomal` executable in the same directory as `align.py`, rather than in
`$PATH`.

On mac you will need to compile using `gcc` because `clang` does not support `openmp`:
```
    brew install gcc
    export CC=/usr/local/bin/gcc-8
```
Change `CC` to match your settings if necessary. Then proceed to build and install normally.

## Using

There are three main ways of using `eflomal`:

 1. Directly call the `eflomal` binary. Note that this requires some
    preprocessing.
 2. Use the [align.py](./align.py) command-line interface, which is partly
    compatible with that of `efmaral`. Run `python3 align.py --help` for
    instructions.
 3. Use the Cython module to call the `eflomal` binary, this takes care of
    the preprocessing and file conversions necessary. See the docstrings
    in [eflomal.pyx](./python/eflomal/eflomal.pyx) for documentation.

In addition, there are convenience scripts for aligning and symmetrizing (with
the `atools` program from `fast_align`) as well as evaluating with data from
the WPT shared task datasets. These work the same way as in `efmaral`,
please see its
[README](https://github.com/robertostling/efmaral/blob/master/README.md) for
details.

## Data format

The `align.py` interface expects one sentence per line with space-separated
tokens, similar to most word alignment software.

## Performance

This is a comparison between eflomal,
[efmaral](https://github.com/robertostling/efmaral) and fast_align.

The difference between efmaral and eflomal is in part due to different default
parameters, in particular the number of iterations and the number of
independent samplers.

Note that all timing figures below include alignments in both directions
(run in parallel) and symmetrization.

### eflomal

| Languages | Sentences | AER | CPU time (s) | Real time (s) |
| --------- | ---------:| ---:| ------------:| -------------:|
| English-French | 1,130,551 | 0.081 | 1,232 | 337 |
| English-Inkutitut | 340,601 | 0.203 | 161 | 44 |
| Romanian-English | 48,681 | 0.298 | 159 | 33 |
| English-Hindi | 3,530 | 0.467 | 31 | 6 |

### efmaral

| Languages | Sentences | AER | CPU time (s) | Real time (s) |
| --------- | ---------:| ---:| ------------:| -------------:|
| English-Swedish | 1,862,426 | 0.133 | 1,719 | 620 |
| English-French | 1,130,551 | 0.085 | 763 | 279 |
| English-Inkutitut | 340,601 | 0.235 | 122 | 46 |
| Romanian-English | 48,681 | 0.287 | 161 | 46 |
| English-Hindi | 3,530 | 0.483 | 98 | 10 |

### fast_align

| Languages | Sentences | AER | CPU time (s) | Real time (s) |
| --------- | ---------:| ---:| ------------:| -------------:|
| English-Swedish | 1,862,426 | 0.205 | 11,090 | 672 |
| English-French | 1,130,551 | 0.153 | 3,840 | 241 |
| English-Inuktitut | 340,601 | 0.287 | 477 | 47 |
| Romanian-English | 48,681 | 0.325 | 208 | 17 |
| English-Hindi | 3,530 | 0.672 | 24 | 2 |


