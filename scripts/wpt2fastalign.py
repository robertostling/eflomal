#!/usr/bin/env python3

# Script to convert two-file aligned corpora to the format expected by
# fastalign.
#
# Sentences that are blank in either language are silently dropped.
#
# Usage: ./wpt2fastalign.py file.eng file.deu >file.eng-deu

import sys

if len(sys.argv) != 3:
    print('Usage %s file.eng file.deu >file.eng-deu' % __file__,
          file=sys.stderr)
    sys.exit()

with open(sys.argv[1], 'r') as f: lines1 = [s.rstrip('\n') for s in f]
with open(sys.argv[2], 'r') as f: lines2 = [s.rstrip('\n') for s in f]
assert len(lines1) == len(lines2)
for line1, line2 in zip(lines1, lines2):
    if line1 and line2:
        print(line1 + ' ||| ' + line2)

