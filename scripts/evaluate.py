#!/usr/bin/env python3

# Script to evaluate efmaral or fast_align using WPT shared task data sets.
#
#   time python3 scripts/evaluate.py efmaral test.eng.hin.wa \
#       test.eng test.hin training.eng training.hin
#
# or, to use fast_align:
#
#   time python3 scripts/evaluate.py fast_align test.eng.hin.wa \
#       test.eng test.hin training.eng training.hin
#
# atools (from the fast_align package) must be installed and in $PATH

import re, sys, subprocess, os
from multiprocessing import Pool
from tempfile import NamedTemporaryFile

RE_NUMBERED = re.compile(r'<s snum=(\d+)>(.*?)</s>\s*$')

def wpteval(align, train_filenames, test_filename, gold_wa):
    test_numbers = []

    mosesf = NamedTemporaryFile('w+', encoding='utf-8')

    with NamedTemporaryFile('w', encoding='utf-8') as outf1, \
         NamedTemporaryFile('w', encoding='utf-8') as outf2:
        with open(test_filename[0], 'r', encoding='utf-8') as f:
            for i,line in enumerate(f):
                m = RE_NUMBERED.match(line)
                assert m, 'Test data file %s not numbered properly!' % \
                          test_filename[0]
                test_numbers.append(m.group(1))
                print(m.group(2).strip(), file=outf1)
        with open(test_filename[1], 'r', encoding='utf-8') as f:
            for i,line in enumerate(f):
                m = RE_NUMBERED.match(line)
                assert m, 'Test data file %s not numbered properly!' % \
                          test_filename[1]
                assert test_numbers[i] == m.group(1)
                print(m.group(2).strip(), file=outf2)

        for filename1,filename2 in train_filenames:
            with open(filename1, 'r', encoding='utf-8') as f1, \
                 open(filename2, 'r', encoding='utf-8') as f2:
                while True:
                    line1 = f1.readline()
                    line2 = f2.readline()
                    assert (not line1) == (not line2), \
                           'Number of lines differs between %s and %s!' % (
                           filename1, filename2)
                    if (not line1) or (not line2): break
                    line1 = line1.strip()
                    line2 = line2.strip()
                    if line1 and line2:
                        print(line1, file=outf1)
                        print(line2, file=outf2)

        outf1.flush()
        outf2.flush()
        align(outf1.name, outf2.name, mosesf.name)

    with NamedTemporaryFile('w', encoding='utf-8') as outf:
        for lineno in test_numbers:
            for i,j in map(lambda s: s.split('-'), mosesf.readline().split()):
                print('%s %d %d' % (lineno, int(i)+1, int(j)+1), file=outf)

        outf.flush()
        subprocess.call(
                ['perl', '3rdparty/wa_check_align.pl', outf.name])
        subprocess.call(
                ['perl', '3rdparty/wa_eval_align.pl', gold_wa, outf.name])

    mosesf.close()


def fastalign(args):
    in_filename, out_filename, reverse = args
    with open(out_filename, 'w') as outf:
        subprocess.call(
            ['fast_align', '-i', in_filename, '-d', '-o', '-v']
            if reverse else 
            ['fast_align', '-i', in_filename, '-d', '-o', '-v', '-r'],
            stdout=outf)


def main():
    symmetrization = 'grow-diag-final-and'
    if len(sys.argv) >= 8 and sys.argv[7] == '--symmetrization':
        symmetrization = sys.argv[8]
        extra_opts = sys.argv[9:]
    else:
        extra_opts = sys.argv[7:]

    def align_efmaral(text1, text2, output):
        subprocess.call(['scripts/align_symmetrize.sh', text1, text2, output,
                         symmetrization] + extra_opts)

    def align_fastalign(text1, text2, output):
        with NamedTemporaryFile('w', encoding='utf-8') as outf, \
             NamedTemporaryFile('w', encoding='utf-8') as fwdf, \
             NamedTemporaryFile('w', encoding='utf-8') as backf:
            subprocess.call(['scripts/wpt2fastalign.py', text1, text2],
                            stdout=outf)
            outf.flush()

            with Pool(2) as p:
                r = p.map(fastalign,
                          [(outf.name, fwdf.name, False),
                           (outf.name, backf.name, True)])

            with open(output, 'w') as outputf:
                subprocess.call(['atools', '-i', fwdf.name, '-j', backf.name,
                                 '-c', symmetrization], stdout=outputf)

    aligner = align_efmaral if sys.argv[1] in ('efmaral', 'eflomal') \
              else align_fastalign
    wpteval(aligner,
            zip(sys.argv[5].split(','), sys.argv[6].split(',')),
            (sys.argv[3], sys.argv[4]),
            sys.argv[2])

if __name__ == '__main__': main()

