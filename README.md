# eflomal
Efficient Low-Memory Aligner

This is a word alignment tool based on
[efmaral](https://github.com/robertostling/efmaral), with the following main
differences:
 * More compact data structures are used, so memory requirements are much
   lower (by orders of magnitude), at the cost of increasing the runtime by
   about three times (which still compares favorably to e.g. `fast_align`).
 * Simulated annealing is used instead of sampling alignment variable
   marginals, this also reduces memory consumption at the cost of some
   accuracy.

Technical details relevant to both `efmaral` and `eflomal` can be found in
the following article:
 * [Östling and Tiedemann (2016)](https://ufal.mff.cuni.cz/pbml/106/art-ostling-tiedemann.pdf) ([BibTeX](http://www.robos.org/sections/research/robert_bib.html#Ostling2016efmaral)).

## Installing

To compile and install the C binary:

    make
    sudo make install

edit `Makefile` manually if you want to install somewhere other than the
default `/usr/local/bin`.

The Python library can be installed as follows:

    python3 setup.py install --user

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

