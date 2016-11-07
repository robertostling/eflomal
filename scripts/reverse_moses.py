import sys
import os

def reverse(filename):
    for i in range(1000):
        backup = '%s.bak%d' % (filename, i)
        if not os.path.exists(backup): break
    os.rename(filename, backup)
    with open(filename, 'w') as outf, open(backup, 'r') as inf:
        for line in inf:
            links = [s.split('-') for s in line.split()]
            if not all(len(t) == 2 for t in links):
                raise ValueError('Links must be formatted as I-J')
            links = sorted((int(j), int(i)) for i,j in links)
            outf.write(' '.join('%d-%d' % (j,i) for j,i in links) + '\n')

if __name__ == '__main__':
    for filename in sys.argv[1:]: reverse(filename)

