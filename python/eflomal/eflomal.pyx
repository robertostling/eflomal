cimport cython
from cpython cimport bool
cimport numpy as np
from libc.stdio cimport fprintf, fdopen, fputc, fflush, FILE

import os
import sys
import math
import subprocess
from tempfile import NamedTemporaryFile

import numpy as np
from scipy.sparse import coo_matrix


cpdef write_text(pyfile, tuple sents, int voc_size):
    """Write a sequence of sentences in the format expected by eflomal

    Arguments:
    pyfile -- Python file object to write to
    sents -- tuple of sentences, each encoded as np.ndarray(uint32)
    voc_size -- size of vocabulary
    """
    cdef int token, i, n
    cdef FILE *f
    cdef np.ndarray[np.uint32_t, ndim=1] sent

    f = fdopen(pyfile.fileno(), 'wb')
    fprintf(f, '%d %d\n', len(sents), voc_size)
    for sent in sents:
        n = len(sent)
        i = 0
        fprintf(f, '%d', n)
        while i < n:
            fprintf(f, ' %d', sent[i])
            i += 1
        fputc(10, f)
    fflush(f)


cpdef read_links(pyfile, int n_sents):
    """Read the links produced by eflomal with the -l argument

    Arguments:
    pyfile -- Python file object to read from
    n_sents -- number of sentences to read

    Returns:
    alignments -- tuple of np.ndarray(uint16) of the same length as the
                  target sentence where each elements is the 0-based position
                  in the corresponding source sentence, or 0xffff in case of
                  a NULL alignment.
    """
    cdef:
        int i, j, length, x
        bytes line, s
        list fields, alignments
        np.ndarray[np.uint16_t, ndim=1] links

    alignments = [None]*n_sents
    for i in range(n_sents):
        line = pyfile.readline()
        fields = line.split()
        length = len(fields)
        links = np.empty(length, np.uint16)
        for j in range(length):
            x = int(fields[j])
            links[j] = 0xffff if x == -1 else x
        alignments[i] = links

    return tuple(alignments)


def align(
        tuple src_sents,
        tuple trg_sents,
        int src_voc_size,
        int trg_voc_size,
        str links_filename=None,
        str statistics_filename=None,
        str scores_filename=None,
        bool return_links=False,
        int model=3,
        tuple n_iterations=None,
        int annealing_iterations=0,
        int clean_sentences=0,
        bool quiet=True,
        double rel_iterations=1.0):
    """Call the eflomal binary to perform word alignment

    Arguments:
    src_sents -- tuple of np.ndarray(uint32) with source sentences
    trg_sents -- tuple of np.ndarray(uint32) with target_sentences
    src_voc_size -- size of source vocabulary
    trg_voc_size -- size of target vocabulary
    links_filename -- if given, write links here
    statistics_filename -- if given, write alignment statistics here
    scores_filename -- if given, write sentence alignment scoeres here
    return_links -- if true, a tuple with links (from read_links) will be
                    added to the tuple returned from this function
    model -- alignment model (1 = IBM1, 2 = HMM, 3 = HMM+fertility)
    n_iterations -- 3-tuple with number of iterations per model, if this is
                    not given the numbers will be computed automatically based
                    on rel_iterations
    annealing_iterations -- number of simulated annealing iterations
    clean_sentences -- if given, assume that only the first _clean_sentences_
                       sentences in the data are truly parallel, this is
                       useful mostly to append candidate sentence pairs to
                       known parallel pairs for scoring them
    quiet -- if True, suppress output
    rel_iterations -- number of iterations relative to the default
    """

    assert len(src_sents) == len(trg_sents)

    if n_iterations is None:
        iters = max(1, int(rel_iterations*10000 / math.sqrt(len(src_sents))))
        if model == 1:
            n_iterations = (iters, 0, 0)
        elif model == 2:
            n_iterations = (max(1, iters//4), iters, 0)
        else:
            n_iterations = (max(1, iters//4), max(1, iters//4), iters)

    remove_links_filename = False
    if return_links and links_filename is None:
        with NamedTemporaryFile('wb', delete=False) as f:
            links_filename = f.name
            remove_links_filename = True

    with NamedTemporaryFile('wb') as srcf, \
         NamedTemporaryFile('wb') as trgf:
        write_text(srcf, src_sents, src_voc_size)
        write_text(trgf, trg_sents, trg_voc_size)
        args = ['eflomal',
                '-m', str(model),
                '-s', srcf.name,
                '-t', trgf.name,
                '-n', str(clean_sentences),
                '-1', str(n_iterations[0])]
        if quiet: args.append('-q')
        if model >= 2: args.extend(['-2', str(n_iterations[1])])
        if model >= 3: args.extend(['-3', str(n_iterations[2])])
        if links_filename: args.extend(['-l', links_filename])
        if statistics_filename: args.extend(['-v', statistics_filename])
        if scores_filename: args.extend(['-s', scores_filename])
        if not quiet: sys.stderr.write(' '.join(args) + '\n')
        #args = ['gdb', '-ex=run', '--args'] + args
        subprocess.call(args)

    retval = []

    if return_links:
        with open(links_filename, 'rb') as f:
            retval.append(read_links(f, len(trg_sents)))
        if remove_links_filename:
            os.remove(links_filename)

    return tuple(retval)

