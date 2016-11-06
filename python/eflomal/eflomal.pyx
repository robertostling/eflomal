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


cpdef tuple read_text(pyfile, bool lowercase, int prefix_len, int suffix_len):
    """Read a tokenized text file as a list of indexed sentences.
    
    Optionally transform the vocabulary according to the parameters.
    
    pyfile -- file to read
    lowercase -- if True, all tokens are lowercased
    prefix_len -- if non-zero, all tokens are cut of after so many characters
    suffix_len -- if non-zero, as above, but cutting from the right side

    Returns:
    a tuple (list sents, dict index) containing the actual sentences and the
    string-to-index mapping used.
    """
    cdef:
        np.ndarray[np.uint32_t, ndim=1] sent
        list sents, tokens
        str line, token
        dict index
        int i, n, idx

    index = {}
    sents = []
    for line in pyfile:
        if lowercase:
            tokens = line.lower().split()
        else:
            tokens = line.split()
        n = len(tokens)
        sent = np.empty(n, dtype=np.uint32)

        for i in range(n):
            token = tokens[i]
            if prefix_len != 0: token = token[:prefix_len]
            elif suffix_len != 0: token = token[-suffix_len:]
            idx = index.get(token, -1)
            if idx == -1:
                idx = len(index)
                index[token] = idx
            sent[i] = idx

        sents.append(sent)

    return (sents, index)


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
        if n < 0x400:
            i = 0
            fprintf(f, '%d', n)
            while i < n:
                fprintf(f, ' %d', sent[i])
                i += 1
            fputc(10, f)
        else:
            fprintf(f, '0\n')
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
        bool moses_format=False,
        int model=3,
        bool reverse=False,
        tuple n_iterations=None,
        int annealing_iterations=0,
        int argmax_samples=-1,
        int n_samplers=1,
        int clean_sentences=0,
        bool quiet=True,
        double rel_iterations=1.0,
        bool use_gdb=False):
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
    moses_format -- if true, use the Moses-style format with srcidx-trgidx
                    tuples rather than one index per target token (and -1 for
                    NULL links)
    model -- alignment model (1 = IBM1, 2 = HMM, 3 = HMM+fertility)
    reverse -- if both this and moses_format is true, reverse the direction
               of the alignments written
    n_iterations -- 3-tuple with number of iterations per model, if this is
                    not given the numbers will be computed automatically based
                    on rel_iterations
    annealing_iterations -- number of simulated annealing iterations
    argmax_samples -- number of per-sentence samples before performing
                      final argmax operation
    n_samplers -- number of independent samplers to run
    clean_sentences -- if given, assume that only the first _clean_sentences_
                       sentences in the data are truly parallel, this is
                       useful mostly to append candidate sentence pairs to
                       known parallel pairs for scoring them
    quiet -- if True, suppress output
    rel_iterations -- number of iterations relative to the default
    """

    assert len(src_sents) == len(trg_sents)
    n_sents = len(src_sents)

    if n_iterations is None:
        iters = max(1, int(rel_iterations*2500 / math.sqrt(len(src_sents))))
        iters4 = max(1, iters//4)
        if argmax_samples < 0:
            argmax_samples = 1 # max(1, iters//2)
            iters = max(2, iters-argmax_samples)
        if model == 1:
            n_iterations = (iters, 0, 0)
        elif model == 2:
            n_iterations = (iters4, iters, 0)
        else:
            n_iterations = (iters4, iters4, iters)
    elif argmax_samples < 0:
        argmax_samples = 1

    remove_links_filename = False
    if return_links and links_filename is None:
        with NamedTemporaryFile('wb', delete=False) as f:
            links_filename = f.name
            remove_links_filename = True

    with NamedTemporaryFile('wb') as srcf, \
         NamedTemporaryFile('wb') as trgf:
        write_text(srcf, src_sents, src_voc_size)
        src_sents = None
        write_text(trgf, trg_sents, trg_voc_size)
        trg_sents = None
        args = ['eflomal',
                '-m', str(model),
                '-s', srcf.name,
                '-t', trgf.name,
                '-n', str(clean_sentences),
                '-g', str(argmax_samples),
                '-i', str(n_samplers),
                '-1', str(n_iterations[0])]
        if reverse: args.append('-r')
        if quiet: args.append('-q')
        if moses_format: args.append('-e')
        if annealing_iterations: args.extend(['-a', str(annealing_iterations)])
        if model >= 2: args.extend(['-2', str(n_iterations[1])])
        if model >= 3: args.extend(['-3', str(n_iterations[2])])
        if links_filename: args.extend(['-l', links_filename])
        if statistics_filename: args.extend(['-v', statistics_filename])
        if scores_filename: args.extend(['-s', scores_filename])
        if not quiet: sys.stderr.write(' '.join(args) + '\n')
        if use_gdb: args = ['gdb', '-ex=run', '--args'] + args
        subprocess.call(args)

    retval = []

    if return_links:
        with open(links_filename, 'rb') as f:
            retval.append(read_links(f, n_sents))
        if remove_links_filename:
            os.remove(links_filename)

    return tuple(retval)

