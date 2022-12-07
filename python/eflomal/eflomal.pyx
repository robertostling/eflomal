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
            fputc(48, f)
            fputc(10, f)
    fflush(f)


def align(
        str source_filename,
        str target_filename,
        str links_filename_fwd=None,
        str links_filename_rev=None,
        str statistics_filename=None,
        str scores_filename_fwd=None,
        str scores_filename_rev=None,
        str priors_filename=None,
        int model=3,
        int score_model=0,
        tuple n_iterations=None,
        int n_samplers=1,
        bool quiet=True,
        double rel_iterations=1.0,
        double null_prior=0.2,
        bool use_gdb=False):
    """Call the eflomal binary to perform word alignment

    Arguments:
    source_filename -- str with source text filename, this and the target
                       text should both be written using write_text()
    target_filename -- str with target text filename
    links_filename_fwd -- if given, write links here (forward direction)
    links_filename_rev -- if given, write links here (reverse direction)
    statistics_filename -- if given, write alignment statistics here
    scores_filename -- if given, write sentence alignment scoeres here
    priors_filename -- if given, read Dirichlet priors from here
    model -- alignment model (1 = IBM1, 2 = HMM, 3 = HMM+fertility)
    n_iterations -- 3-tuple with number of iterations per model, if this is
                    not given the numbers will be computed automatically based
                    on rel_iterations
    n_samplers -- number of independent samplers to run
    quiet -- if True, suppress output
    rel_iterations -- number of iterations relative to the default
    """

    with open(source_filename, 'rb') as f:
        n_sentences = int(f.readline().split()[0])

    if n_iterations is None:
        iters = max(2, int(round(
            rel_iterations*5000 / math.sqrt(n_sentences))))
        iters4 = max(1, iters//4)
        if model == 1:
            n_iterations = (iters, 0, 0)
        elif model == 2:
            n_iterations = (max(2, iters4), iters, 0)
        else:
            n_iterations = (max(2, iters4), iters4, iters)

    executable = os.path.join(os.path.dirname(__file__), 'bin', 'eflomal')
    args = [executable,
            '-m', str(model),
            '-s', source_filename,
            '-t', target_filename,
            '-n', str(n_samplers),
            '-N', str(null_prior),
            '-1', str(n_iterations[0])]
    if quiet: args.append('-q')
    if model >= 2: args.extend(['-2', str(n_iterations[1])])
    if model >= 3: args.extend(['-3', str(n_iterations[2])])
    if links_filename_fwd: args.extend(['-f', links_filename_fwd])
    if links_filename_rev: args.extend(['-r', links_filename_rev])
    if statistics_filename: args.extend(['-S', statistics_filename])
    if score_model > 0: args.extend(['-M', str(score_model)])
    if scores_filename_fwd: args.extend(['-F', scores_filename_fwd])
    if scores_filename_rev: args.extend(['-R', scores_filename_rev])
    if priors_filename: args.extend(['-p', priors_filename])
    if not quiet: sys.stderr.write(' '.join(args) + '\n')
    if use_gdb: args = ['gdb', '-ex=run', '--args'] + args
    subprocess.call(args)

