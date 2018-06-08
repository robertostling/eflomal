# -*- coding: utf-8 -*-


class IBM1():
    def __init__(self, p, voc_s, voc_t):
        self.p = p
        self.voc_s = voc_s
        self.voc_t = voc_t
    
    def get(self, word_s, word_t):
        s_index = word_s if type(word_s) == int else self.voc_s.get(word_s, -1)
        t_index = word_t if type(word_t) == int else self.voc_t.get(word_t, -1)
        
        if s_index < 0 or t_index < 0: return 0.0
        
        return self.p[s_index, t_index]
    
    def dump(self, file):
        voc_s_rev = {}
        for w, i in self.voc_s.items():
            voc_s_rev[i] = w
        voc_t_rev = {}
        for w, i in self.voc_t.items():
            voc_t_rev[i] = w
        X,Y = self.p.nonzero()
        for s,t in zip(X,Y):
            file.write("{}\t{}\t{}\n".format(voc_s_rev[s], voc_t_rev[t], self.p[s,t]))
