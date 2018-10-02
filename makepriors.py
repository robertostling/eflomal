#!/usr/bin/env python3

import sys, argparse, os.path
from collections import Counter

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
            '-a', '--alignments', dest='alignments_filename', type=str,
            metavar='filename', required=True,
            help='Alignments file')
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

    with open(args.alignments_filename, 'rb') as af:
        if args.joint_filename:
            with open(args.joint_filename, 'r', encoding='utf-8') as f:
                for lineno, (line, a_line) in enumerate(zip(f, af)):
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
                    links = [tuple(map(int, s.split(b'-')))
                             for s in a_line.split()]
                    for i, j in links:
                        if i >= len(src_sent) or j >= len(trg_sent):
                            print('ERROR: alignment out of bounds in line %d: '
                                  '(%d, %d)' % (lineno+1, i, j),
                                  file=sys.stderr, flush=True)
                            sys.exit(1)
                        priors[(src_sent[i], trg_sent[j])] += 1

           # TODO: confirm EOF in both files
        else:
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
        print('%s\t%s\t%g' % (src, trg, alpha), file=priorsf)
    priorsf.close()


if __name__ == '__main__': main()

