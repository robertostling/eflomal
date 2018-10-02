import sys

with open(sys.argv[1], 'r') as srcf, open(sys.argv[2], 'r') as trgf:
    for src, trg in zip(srcf, trgf):
        src = src.strip()
        trg = trg.strip()
        if src and trg:
            print(src + ' ||| ' + trg)

