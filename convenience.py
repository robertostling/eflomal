# -*- coding: utf-8 -*-

import logging
import gzip


def log_levels_mapping(verbose):
    if verbose==0: return logging.WARNING
    if verbose==1: return logging.INFO
    if verbose>=2: return logging.DEBUG


logger = logging.getLogger(__name__)
handler = logging.StreamHandler()
logger.addHandler(handler)


def error(msg, code=1):
    """Log an error message and exit with given code (default: 1)."""
    logger.error(msg)
    exit(code)


class bcolors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    ORANGE = '\033[38;5;214m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def header(text):
    return bcolors.HEADER+text+bcolors.ENDC


def blue(text):
    return bcolors.BLUE+text+bcolors.ENDC


def green(text):
    return bcolors.GREEN+text+bcolors.ENDC


def yellow(text):
    return bcolors.YELLOW+text+bcolors.ENDC


def orange(text):
    return bcolors.ORANGE+text+bcolors.ENDC


def red(text):
    return bcolors.RED+text+bcolors.ENDC


def bold(text):
    return bcolors.BOLD+text+bcolors.ENDC


def underline(text):
    return bcolors.UNDERLINE+text+bcolors.ENDC


class XFile():
    def __init__(self, f, encoding="utf8"):
        self.encoding = encoding
        self.file = f
    
    def __iter__(self):
        return self
    
    def __next__(self):
        line = self.readline()
        if line=="":
            raise StopIteration
        else:
            return line
    
    def __enter__(self):
        return self
    
    def __exit__(self, arg1, arg2, arg3):
        return self.file.__exit__(arg1, arg2, arg3)
    
    def close(self):
        self.file.close()
    
    def write(self, line):
        if isinstance(self.file, gzip.GzipFile) and hasattr(line, "encode"):
            return self.file.write(line.encode(self.encoding))
        else:
            return self.file.write(line)
    
    def read(self, size=-1):
        line = self.file.read(size)
        try:
            return line.decode(encoding=self.encoding) if type(line)==bytes and not self.mode.endswith("b") else line
        except:
            return line
    
    def readline(self, size=-1):
        line = self.file.readline(size)
        try:
            return line.decode(self.encoding) if type(line)==bytes and not self.mode.endswith("b") else line
        except:
            return line
    
    def readlines(self, hint=-1):
        lines = self.file.readlines(hint)
        if isinstance(self.file, gzip.GzipFile) and not self.mode.endswith("b"):
            try:
                return [l.decode(self.encoding) for l in lines]
            except:
                return lines
        else:
            return lines


def xopen(fname, mode="r", encoding="utf8"):
    if fname.endswith(".gz") or mode.endswith("b"):
        return XFile(gzip.open(fname, mode=mode), encoding)
    else:
        return XFile(open(fname, mode, encoding=encoding), encoding)
