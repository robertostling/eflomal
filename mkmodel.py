#!/usr/bin/env python3

from eflomal import read_text, write_text, align

import sys, argparse, random, os
from tempfile import NamedTemporaryFile

from scipy.sparse import lil_matrix

def compute_counts_fwd(voc_s, voc_t, src_sents, trg_sents, alignment_filename, lowercase):
    counts = lil_matrix(len(voc_s.items()), len(voc_t.items()))
    return counts

def compute_counts_fwd(voc_s, voc_t, src_sents, trg_sents, alignment_filename, lowercase):
    counts = lil_matrix(len(voc_t.items()), len(voc_s.items()))
    return counts

def make_voc(sentences, lowercase=False):
    voc = {}
    index = 0
    for sent in sentences:
        if lowercase: sent = sent.lower()
        for token in sent.split():
            if token not in voc:
                voc[token] = index
                index += 1
    return voc

def main():
    parser = argparse.ArgumentParser(
        description='mkmodel.py: compute IBM-1 translation probabilties using eflomal, the efficient low-memory aligner')
    parser.add_argument(
        '-v', '--verbose', dest='verbose',
        action='store_true', help='Enable verbose output')
    parser.add_argument(
        '--debug', dest='debug',
        action='store_true', help='Enable gdb debugging of eflomal binary')
    parser.add_argument(
        '--no-lowercase', dest='lowercase',
        action='store_false', default=True, help='Do not lowercase input text')
    parser.add_argument(
        '--overwrite', dest='overwrite',
        action='store_true', help='Overwrite existing output files')
    parser.add_argument(
        '--null-prior', dest='null_prior', default=0.2, metavar='X',
        type=float, help='Prior probability of NULL alignment')
    parser.add_argument(
        '-m', '--model', dest='model', default=3, metavar='N',
        type=int, help='Model (1 = IBM1, 2 = IBM1+HMM, 3 = IBM1+HMM+fertility)')
    parser.add_argument(
        '--source-prefix', dest='source_prefix_len', default=0, metavar='N',
        type=int, help='Length of prefix for stemming (source)')
    parser.add_argument(
        '--source-suffix', dest='source_suffix_len', default=0, metavar='N',
        type=int, help='Length of suffix for stemming (source)')
    parser.add_argument(
        '--target-prefix', dest='target_prefix_len', default=0, metavar='N',
        type=int, help='Length of prefix for stemming (target)')
    parser.add_argument(
        '--target-suffix', dest='target_suffix_len', default=0, metavar='N',
        type=int, help='Length of suffix for stemming (target)')
    parser.add_argument(
        '-l', '--length', dest='length', default=1.0, metavar='X',
        type=float, help='Relative number of sampling iterations')
    parser.add_argument(
        '-1', '--ibm1-iters', dest='iters1', default=None, metavar='X',
        type=int, help='Number of IBM1 iterations (overrides --length)')
    parser.add_argument(
        '-2', '--hmm-iters', dest='iters2', default=None, metavar='X',
        type=int, help='Number of HMM iterations (overrides --length)')
    parser.add_argument(
        '-3', '--fert-iters', dest='iters3', default=None, metavar='X',
        type=int,
        help='Number of HMM+fertility iterations (overrides --length)')
    parser.add_argument(
        '--n-samplers', dest='n_samplers', default=3, metavar='X',
        type=int, help='Number of independent samplers to run')
    parser.add_argument(
        '-s', '--source', dest='source_filename', type=str, metavar='filename',
        help='Source text filename', required=True)
    parser.add_argument(
        '-t', '--target', dest='target_filename', type=str, metavar='filename',
        help='Target text filename', required=True)
    parser.add_argument(
        '-f', '--forward-probabilities', dest='p_filename_fwd', type=str,
        metavar='filename',
        help='Filename to write forward direction probabilities to')
    parser.add_argument(
        '-r', '--reverse-probabilities', dest='p_filename_rev', type=str,
        metavar='filename',
        help='Filename to write reverse direction probabilities to')

    args = parser.parse_args()

    if args.p_filename_fwd is None and args.p_filename_rev is None:
        print('ERROR: no file to save probabilities (-f/-r), will do nothing.',
              file=sys.stderr, flush=True)
        sys.exit(1)

    for filename in (args.source_filename, args.target_filename):
        if not os.path.exists(filename):
            print('ERROR: input file %s does not exist!' % filename,
                  file=sys.stderr, flush=True)
            sys.exit(1)

    for filename in (args.p_filename_fwd, args.p_filename_rev):
        if (not args.overwrite) and (filename is not None) \
                and os.path.exists(filename):
            print('ERROR: output file %s exists, will not overwrite!' % \
                    filename,
                  file=sys.stderr, flush=True)
            sys.exit(1)

    if args.verbose:
        print('Reading source text from %s...' % args.source_filename,
              file=sys.stderr, flush=True)
    with open(args.source_filename, 'r', encoding='utf-8') as f:
        src_sents, src_index = read_text(
                f, args.lowercase, args.source_prefix_len, args.source_suffix_len)
        n_src_sents = len(src_sents)
        src_voc_size = len(src_index)
        src_index = None
        srcf = NamedTemporaryFile('wb')
        write_text(srcf, tuple(src_sents), src_voc_size)

    if args.verbose:
        print('Reading target text from %s...' % args.target_filename,
              file=sys.stderr, flush=True)
    with open(args.target_filename, 'r', encoding='utf-8') as f:
        trg_sents, trg_index = read_text(
                f, args.lowercase, args.target_prefix_len, args.target_suffix_len)
        trg_voc_size = len(trg_index)
        n_trg_sents = len(trg_sents)
        trg_index = None
        trgf = NamedTemporaryFile('wb')
        write_text(trgf, tuple(trg_sents), trg_voc_size)

    if n_src_sents != n_trg_sents:
        print('ERROR: number of sentences differ in input files (%d vs %d)' % (
                n_src_sents, n_trg_sents),
              file=sys.stderr, flush=True)
        sys.exit(1)

    iters = (args.iters1, args.iters2, args.iters3)
    if any(x is None for x in iters[:args.model]):
        iters = None

    if args.verbose:
        print('Aligning %d sentences...' % n_src_sents,
              file=sys.stderr, flush=True)
    
    fwd_alignment_file = NamedTemporaryFile('w')
    rev_alignment_file = NamedTemporaryFile('w')

    align(srcf.name, trgf.name,
          links_filename_fwd=fwd_alignment_file.name,
          links_filename_rev=rev_alignment_file.name,
          statistics_filename=None,
          scores_filename=None,
          model=args.model,
          n_iterations=iters,
          n_samplers=args.n_samplers,
          quiet=not args.verbose,
          rel_iterations=args.length,
          null_prior=args.null_prior,
          use_gdb=args.debug)

    srcf.close()
    trgf.close()
    
    voc_s = make_voc(src_sents, args.lowercase)
    voc_t = make_voc(trg_sents, args.lowercase)
    
    if args.p_filename_fwd is not None:
        counts = compute_counts_fwd(voc_s, voc_t, src_sents, trg_sents, fwd_alignment_file.name, args.lowercase)
        p = compute_p(voc_s, voc_t, counts)
        save_p(p, voc_s, voc_t, args.p_filename_fwd)
    
    if args.p_filename_rev is not None:
        counts = compute_counts_rev(voc_s, voc_t, src_sents, trg_sents, rev_alignment_file.name, args.lowercase)
        p = compute_p(voc_t, voc_s, counts)
        save_p(p, voc_t, voc_s, args.p_filename_rev)

    fwd_alignment_file.close()
    rev_alignment_file.close()


if __name__ == '__main__': main()

