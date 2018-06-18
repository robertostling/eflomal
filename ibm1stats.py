#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import pickle

from convenience import xopen
from convenience import Logger
from convenience import header, blue, green, yellow, orange, red, bold, underline

if __name__=="__main__":
    parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("input", help="Input model file. Pickle or text, gzipped or not. Automatically processed extensions are .pickle, .pickle.gz, .gz. Required.")
    parser.add_argument("--delimiter", "-d", type=str, dest="delimiter", default="\t",
                        help="Delimiter used in model file, if in text format. Use plain string between quotes. Default=<tab>.")
    parser.add_argument("-v", "--verbosity", action="count", default=0, help="increase verbosity")
    args = parser.parse_args()
    logger = Logger(args.verbosity)
    
    logger.info("Loading model")
    
    if args.input.endswith(".pickle.gz") or args.input.endswith(".pickle"):
        logger.debug("Pickle detected")
        with xopen(args.input, "rb") as f:
            model = pickle.load(f)
        print(blue("Source vocabulary size:\t"+bold(str(len(model.voc_s)))))
        print(blue("Target vocabulary size:\t"+bold(str(len(model.voc_t)))))
        print(green("Number of entries:\t"+bold(str(model.p.count_nonzero()))))
    
    else:
        logger.debug("Text format detected")
        with xopen(args.input, "r") as f:
            n_entries = 0
            voc_s = set()
            voc_t = set()
            for line in f.readlines():
                entry = line.split(args.delimiter, maxsplit=2)
                voc_s.add(entry[0])
                voc_t.add(entry[1])
                n_entries += 1

        print(blue("Source vocabulary size:\t"+bold(str(len(voc_s)))))
        print(blue("Target vocabulary size:\t"+bold(str(len(voc_t)))))
        print(green("Number of entries:\t"+bold(str(n_entries))))
