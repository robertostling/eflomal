#!/usr/bin/env python3

from eflomal import read_text, align

import sys, argparse, random, os

def main():
    parser = argparse.ArgumentParser(
        description='eflomal: efficient low-memory aligner')
    parser.add_argument(
        '-v', '--verbose', dest='verbose',
        action='store_true', help='Enable verbose output')
    parser.add_argument(
        '--debug', dest='debug',
        action='store_true', help='Enable gdb debugging of eflomal binary')
    parser.add_argument(
        '--overwrite', dest='overwrite',
        action='store_true', help='Overwrite existing output files')
    parser.add_argument(
        '-r', '--reverse', dest='reverse',
        action='store_true', help='Align in the reverse direction')
    parser.add_argument(
        '-p', '--plain', dest='plain',
        action='store_true', help='Use plain output format rather than Moses')
    parser.add_argument(
        '--null-prior', dest='null_prior', default=0.2, metavar='X',
        type=float, help='Prior probability of NULL alignment')
    parser.add_argument(
        '--seed', dest='seed', default=None,
        type=int, help='Random seed')
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
        type=int, help='Number of HMM+fertility iterations (overrides --length)')
    parser.add_argument(
        '-a', '--anneal', dest='annealing_iters', default=0, metavar='X',
        type=int, help='Number of annealing iterations')
    parser.add_argument(
        '--argmax-samples', dest='argmax_samples', default=-1, metavar='X',
        type=int, help='Number of per-sentence samples before argmax')
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
        '-o', '--output', dest='links_filename', type=str, metavar='filename',
        help='Filename to write alignments to', required=True)

    args = parser.parse_args()

    #if args.verbose:
    #    from pprint import pprint
    #    pprint(vars(args), stream=sys.stderr)

    seed = random.randint(0, 0x7ffffff) if args.seed is None else args.seed

    for filename in (args.source_filename, args.target_filename):
        if not os.path.exists(filename):
            print('ERROR: input file %s does not exist!' % filename,
                  file=sys.stderr, flush=True)
            sys.exit(1)

    if (not args.overwrite) and os.path.exists(args.links_filename):
        print('ERROR: output file %s exists, will not overwrite!' % \
                args.links_filename,
              file=sys.stderr, flush=True)
        sys.exit(1)

    if args.verbose:
        print('Reading source text from %s...' % args.source_filename,
              file=sys.stderr, flush=True)
    with open(args.source_filename, 'r', encoding='utf-8') as f:
        src_sents, src_index = read_text(
                f, True, args.source_prefix_len, args.source_suffix_len)
        src_voc_size = len(src_index)
        src_index = None

    if args.verbose:
        print('Reading target text from %s...' % args.target_filename,
              file=sys.stderr, flush=True)
    with open(args.target_filename, 'r', encoding='utf-8') as f:
        trg_sents, trg_index = read_text(
                f, True, args.target_prefix_len, args.target_suffix_len)
        trg_voc_size = len(trg_index)
        trg_index = None

    if len(src_sents) != len(trg_sents):
        print('ERROR: number of sentences differ in input files (%d vs %d)' % (
                len(src_sents), len(trg_sents)),
              file=sys.stderr, flush=True)
        sys.exit(1)

    iters = (args.iters1, args.iters2, args.iters3)

    if args.verbose:
        print('Aligning %d sentences...' % len(src_sents),
              file=sys.stderr, flush=True)

    if args.reverse:
        src_sents, trg_sents = trg_sents, src_sents
        src_voc_size, trg_voc_size = trg_voc_size, src_voc_size

    src_sents = tuple(src_sents)
    trg_sents = tuple(trg_sents)

    align(src_sents, trg_sents, src_voc_size, trg_voc_size,
          return_links=False, links_filename=args.links_filename,
          model=args.model, n_iterations=iters if any(iters) else None,
          n_samplers=args.n_samplers,
          annealing_iterations=args.annealing_iters,
          argmax_samples=args.argmax_samples,
          reverse=args.reverse,
          moses_format=not args.plain,
          quiet=not args.verbose, rel_iterations=args.length,
          use_gdb=args.debug)


if __name__ == '__main__': main()

