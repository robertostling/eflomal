# eflomal
Efficient Low-Memory Aligner

This is a word alignment tool based on
[efmaral](https://github.com/robertostling/efmaral), with the following main
differences:
 * More compact data structures are used, so memory requirements are much
   lower (by orders of magnitude), possibly at the cost of some speed.
 * Simulated annealing is used instead of sampling alignment variable
   marginals, this also reduces memory consumption at the cost of some
   accuracy.
 * `eflomal` is a stand-alone C program that is not so convenient to use on
   its own, it is best interfaced from the Python library included.

Technical details can be found in the following article:
 * [Östling and Tiedemann (2016)](https://ufal.mff.cuni.cz/pbml/106/art-ostling-tiedemann.pdf) ([BibTeX](http://www.robos.org/sections/research/robert_bib.html#Ostling2016efmaral)).

## Installing

To compile and install the C binary:

    make
    sudo make install

edit `Makefile` manually if you want to install somewhere other than the
default `/usr/local/bin`.

The Python library can be installed as follows:

    python3 setup.py install --user

