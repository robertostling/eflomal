#!/usr/bin/env python3

import sys, argparse, os.path
from collections import Counter
from operator import itemgetter

def main():
    parser = argparse.ArgumentParser(
            description='Tool for creating priors from alignments')
    parser.add_argument(
            '-s', '--source', dest='source_filename', type=str,
            metavar='filename',
            help='Source text file')
    parser.add_argument(
            '-t', '--target', dest='target_filename', type=str,
            metavar='filename',
            help='Target text file')
    parser.add_argument(
            '-i', '--input', dest='joint_filename', type=str,
            metavar='filename',
            help='fast_align style ||| separated file')
    parser.add_argument(
            '-f', '--forward-alignments', dest='forward_alignments_filename',
            type=str, metavar='filename', required=True,
            help='File containing forward (or symmetrized) alignments, '
                 'may be same file as --reverse-alignments')
    parser.add_argument(
            '-r', '--reverse-alignments', dest='reverse_alignments_filename',
            type=str, metavar='filename', required=True,
            help='File containing reverse (or symmetrized) alignments, '
                 'may be same file as --forward-alignments')
    parser.add_argument(
            '-p', '--priors', dest='priors_filename', type=str,
            metavar='filename', default='-',
            help='File to write priors to (for use with align.py --priors)')


    args = parser.parse_args()

    if not (args.joint_filename or
            (args.source_filename and args.target_filename)):
        print('ERROR: need to specify either -s and -t, or -i',
                file=sys.stderr, flush=True)
        sys.exit(1)

    for filename in ((args.joint_filename,) if args.joint_filename else
                     (args.source_filename, args.target_filename)):
        if not os.path.exists(filename):
            print('ERROR: input file %s does not exist!' % filename,
                  file=sys.stderr, flush=True)
            sys.exit(1)

    priors = Counter()

    hmmf_priors = Counter()
    hmmr_priors = Counter()

    ferf_priors = Counter()
    ferr_priors = Counter()

    with open(args.forward_alignments_filename, 'rb') as fwdf, \
         open(args.reverse_alignments_filename, 'rb') as revf:
        if args.joint_filename:
            with open(args.joint_filename, 'r', encoding='utf-8') as f:
                for lineno, (line, fwd_line, rev_line) in \
                        enumerate(zip(f, fwdf, revf)):
                    fields = line.strip().split(' ||| ')
                    if len(fields) != 2:
                        print('ERROR: line %d of %s does not contain a '
                              'single ||| separator, or sentence(s) are '
                              'empty!' % (
                                  lineno+1, args.joint_filename),
                              file=sys.stderr, flush=True)
                        sys.exit(1)
                    src_sent = fields[0].split()
                    trg_sent = fields[1].split()
                    fwd_links = [tuple(map(int, s.split(b'-')))
                             for s in fwd_line.split()]
                    rev_links = [tuple(map(int, s.split(b'-')))
                             for s in rev_line.split()]
                    for i, j in fwd_links:
                        if i >= len(src_sent) or j >= len(trg_sent):
                            print('ERROR: alignment out of bounds in line %d: '
                                  '(%d, %d)' % (lineno+1, i, j),
                                  file=sys.stderr, flush=True)
                            sys.exit(1)
                        priors[(src_sent[i], trg_sent[j])] += 1

                    last_j = -1
                    last_i = -1
                    for i, j in sorted(fwd_links, key=itemgetter(1)):
                        if j != last_j:
                            hmmf_priors[i - last_i] += 1
                        last_i = i
                        last_j = j
                    hmmf_priors[len(src_sent) - last_i] += 1

                    last_j = -1
                    last_i = -1
                    for i, j in sorted(rev_links, key=itemgetter(0)):
                        if i != last_i:
                            hmmr_priors[j - last_j] += 1
                        last_i = i
                        last_j = j
                    hmmr_priors[len(trg_sent) - last_j] += 1

                    fwd_fert = Counter(i for i, j in fwd_links)
                    rev_fert = Counter(j for i, j in rev_links)
                    for i, fert in fwd_fert.items():
                        ferf_priors[(src_sent[i], fert)] += 1
                    for j, fert in rev_fert.items():
                        ferr_priors[(trg_sent[j], fert)] += 1

           # TODO: confirm EOF in all files
        else:
            raise NotImplementedError('TODO')
            with open(args.source_filename, 'r', encoding='utf-8') as srcf, \
                 open(args.target_filename, 'r', encoding='utf-8') as trgf:
                for lineno, (src_line, trg_line, a_line) in enumerate(
                        zip(srcf, trgf, af)):
                    src_sent = src_line.split()
                    trg_sent = trg_line.split()
                    links = [tuple(map(int, s.split(b'-')))
                             for s in a_line.split()]
                    for i, j in links:
                        if i >= len(src_sent) or j >= len(trg_sent):
                            print('ERROR: alignment out of bounds in line %d: '
                                  '(%d, %d)' % (lineno+1, i, j),
                                  file=sys.stderr, flush=True)
                            sys.exit(1)
                        priors[(src_sent[i], trg_sent[j])] += 1

           # TODO: confirm EOF in all files
 
    priorsf = sys.stdout if args.priors_filename == '-' else \
              open(args.priors_filename, 'w', encoding='utf-8')

    for (src, trg), alpha in sorted(priors.items()):
        print('LEX\t%s\t%s\t%g' % (src, trg, alpha), file=priorsf)

    for (src, fert), alpha in sorted(ferf_priors.items()):
        print('FERF\t%s\t%d\t%g' % (src, fert, alpha), file=priorsf)

    for (trg, fert), alpha in sorted(ferr_priors.items()):
        print('FERR\t%s\t%d\t%g' % (trg, fert, alpha), file=priorsf)

    for jump, alpha in sorted(hmmf_priors.items()):
        print('HMMF\t%d\t%g' % (jump, alpha), file=priorsf)

    for jump, alpha in sorted(hmmr_priors.items()):
        print('HMMR\t%d\t%g' % (jump, alpha), file=priorsf)

    priorsf.close()


if __name__ == '__main__': main()

