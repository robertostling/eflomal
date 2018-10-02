# eflomal
Efficient Low-Memory Aligner

This is a word alignment tool based on
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

## Input data format

When used with the `-s` and `-t` options for separate source/target files, the
`align.py` interface expects one sentence per line with space-separated
tokens, similar to most word alignment software.

The `-i` option assumes a `fast_text` style joint source/target file of the
format
```
source sentence ||| target sentence
another source sentence ||| another target sentence
...
```

The `--priors` option expects a file of the following structure:
```
sourceword1<TAB>targetword1<TAB>alpha1
sourceword2<TAB>targetword2<TAB>alpha2
sourceword3<TAB>targetword3<TAB>alpha3
...
```
where the `alphaN` values will be added to the Dirichlet prior of generating
`targetewordN` from `sourcewordN` (or vice versa, for reverse-direction
alignments). Note that the source and target word will be processed in the
same way as input text, i.e. lower-cased (always) and optionally stemmed
according to the `--source-prefix-len`, `--source-suffix-len`,
`--target-prefix-len`, `--target-suffix-len` options. In other words, you
should be able to pass a raw wordlist to it.

Note that the default value of the Dirichlet prior (defined in
`eflomal.c` as `LEX_ALPHA`) will be *added* to whatever is specified in the
priors file. This means that integer counts for whatever word forms you have
data on are fine in the priors file.

It s possible to use the special `<NULL>` token in the priors file, in case
you want to encourage certain word forms to remain unaligned.
Currently the `makepriors.py` script does not generate these, and this feature
has not been tested yet.

## Generating priors

If you have word alignments from, say, a large dataset you can use these to
generate priors (for use with `align.py` with the `--priors` option) using the
`makepriors.py` script. Assuming you have produced alignments for the file
`en-sv` into `en-sv.a`, you an generate `en-sv.priors` using:

    makepriors.py -i en-sv -a en-sv.a --priors en-sv.priors

Now, if you have another file to align, `en-sv.small`, simply use:

    align.py -i en-sv.small --priors en-sv.priors ... [other options]

## Output data format

The alignment output contains the same number of lines as the input files,
where each line contains pairs of indexes. For instance, if the source input
contains the following:

    a black cat

and the target input is the following:

    kuro neku

the correct output would be:

    1-0 2-1

That is, `1-0` indicates token 1 of the source (black) is aligned to token 0
of the target (kuro), and `2-1` that token 2 of the source (cat) is aligned to
token 1 of the target (neko). `NULL` alignments are not present in the output.

Note that the forward and reverse alignments both use source-target order, so
the output can be fed directly to `atools` (see `scripts/align_symmetrize.sh`
for an example).

In case you made a mistake with the direction, you can fix it afterwards with
`scripts/reverse_moses.py`.

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


