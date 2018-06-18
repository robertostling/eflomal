# -*- coding: utf-8 -*-
"""Implement IBM1 translation model with translation table and estimator."""
from math import pow

class IBM1():
    """Implement IBM1 translation model with translation table and estimator.
    
    Class members:
    voc_s (dict{str -> int}): source vocabulary index
    voc_t (dict{str -> int}): target vocabulary index
    p (scipy.sparse.lil_matrix): source to target translation probabilities
    
    Class methods:
    __init__(self, p, voc_s, voc_t): instantiate an IBM1 object from a matrix and a source and a target vocabulary hash tables
    get: translation probability look up
    estimate: compute phrase translation probability
    estimate_normalized: compute phrase translation probability, normalized by target phrase length
    dump: write out human readable serialization of the translation table
    """
    def __init__(self, p, voc_s, voc_t):
        """Instantiate an IBM1 object.
        
        :param p (scipy.sparse.lil_matrix): translation table stored as sparse matrix
        :param voc_s (dict{str -> int}): source vocabulary index
        :param voc_t (dict{str -> int}): target vocabulary index
        """
        self.p = p
        self.voc_s = voc_s
        self.voc_t = voc_t
    
    def get(self, word_s, word_t):
        """Look up translation probability. Parameters can be strings or indexes.
        
        :param word_s (str or int): source word
        :param word_t (str or int): target word
        :return: translation probability P(word_t|word_s)
        """
        s_index = word_s if type(word_s) == int else self.voc_s.get(word_s, -1)
        t_index = word_t if type(word_t) == int else self.voc_t.get(word_t, -1)
        
        if s_index < 0 or t_index < 0: return 0.0
        
        return self.p[s_index, t_index]
    
    def dump(self, file):
        """Write out human readable serialization of the translation table as TSV file.
        
        :param file: File object opened for writing (works with convenience.XFiles)
        """
        voc_s_rev = {}
        for w, i in self.voc_s.items():
            voc_s_rev[i] = w
        voc_t_rev = {}
        for w, i in self.voc_t.items():
            voc_t_rev[i] = w
        X,Y = self.p.nonzero()
        for s,t in zip(X,Y):
            file.write("{}\t{}\t{}\n".format(voc_s_rev[s], voc_t_rev[t], self.p[s,t]))
    
    def estimate(self, S, T):
        """Compute phrase translation probability according to IBM1 model. P(T|S) = \prod_t \sum_s P(t|s)
        
        :param S: list of source words (str or int)
        :param T: list of target words (str or int)
        :return (float): P(T|S)
        """
        if len(S) == 0:
            return 0.0
        p = 1.0
        for t in T:
            partial = 0.0
            for s in S:
                partial += self.get(s, t)
            p = p * partial
        return p / len(S)
    
    def estimate_normalized(self, S, T):
        """Compute phrase translation probability according to IBM1 model, normalized in order not penalize longer sentences.
        
        Pnorm(T|S) = P(T|S)^len(1/T)
        :param S: list of source words (str or int)
        :param T: list of target words (str or int)
        :return (float): P(T|S)^len(1/T)
        """
        p = self.estimate(S, T)
        if len(T) == 0:
            return p
        else:
            return pow(p, 1/len(T))
