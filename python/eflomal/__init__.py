"""eflomal package"""

from collections import Counter
import logging
from operator import itemgetter
from tempfile import NamedTemporaryFile

from .cython import align, read_text, write_text


logger = logging.getLogger(__name__)


class Aligner:
    """Aligner class"""

    def __init__(self, model=3, score_model=0,
                 n_iterations=None, n_samplers=3,
                 rel_iterations=1.0, null_prior=0.2,
                 source_prefix_len=0, source_suffix_len=0,
                 target_prefix_len=0, target_suffix_len=0):
        self.model = model
        self.score_model = score_model
        self.n_iterations = n_iterations
        self.n_samplers = n_samplers
        self.rel_iterations = rel_iterations
        self.null_prior = null_prior
        self.source_prefix_len = source_prefix_len
        self.source_suffix_len = source_suffix_len
        self.target_prefix_len = target_prefix_len
        self.target_suffix_len = target_suffix_len

    def prepare_files(self, src_input_file, src_output_file,
                      trg_input_file, trg_output_file,
                      priors_input_file, priors_output_file):
        """Convert text files to formats used by eflomal

        Inputs should be file objects or any iterables over lines. Outputs
        should be file objects.

        """
        src_index, n_src_sents, src_voc_size = to_eflomal_text_file(
            src_input_file, src_output_file,
            self.source_prefix_len, self.source_suffix_len)
        trg_index, n_trg_sents, trg_voc_size = to_eflomal_text_file(
            trg_input_file, trg_output_file,
            self.target_prefix_len, self.target_suffix_len)
        if n_src_sents != n_trg_sents:
            logger.error(
                'number of sentences differ in input files (%d vs %d)',
                n_src_sents, n_trg_sents)
            raise ValueError('Mismatched file sizes')
        logger.info('Prepared %d sentences for alignment', n_src_sents)
        if priors_input_file:
            logger.info('Reading lexical priors...')
            priors = read_priors(priors_input_file)
            to_eflomal_priors_file(
                priors, src_index, trg_index, priors_output_file)

    def align(self, src_input, trg_input,
              links_filename_fwd=None, links_filename_rev=None,
              scores_filename_fwd=None, scores_filename_rev=None,
              priors_input=None, quiet=True, use_gdb=False):
        """Run alignment for the input"""
        with NamedTemporaryFile('wb') as srcf, \
             NamedTemporaryFile('wb') as trgf, \
             NamedTemporaryFile('w', encoding='utf-8') as priorsf:
            # Write input files for the eflomal binary
            self.prepare_files(
                src_input, srcf, trg_input, trgf, priors_input, priorsf)
            # Run wrapper for the eflomal binary
            align(srcf.name, trgf.name,
                  links_filename_fwd=links_filename_fwd,
                  links_filename_rev=links_filename_rev,
                  statistics_filename=None,
                  scores_filename_fwd=scores_filename_fwd,
                  scores_filename_rev=scores_filename_rev,
                  priors_filename=(None if priors_input is None
                                   else priorsf.name),
                  model=self.model,
                  score_model=self.score_model,
                  n_iterations=self.n_iterations,
                  n_samplers=self.n_samplers,
                  quiet=quiet,
                  rel_iterations=self.rel_iterations,
                  null_prior=self.null_prior,
                  use_gdb=use_gdb)


class TextIndex:
    """Word to index mapping with lowercasing and prefix/suffix removal"""

    def __init__(self, index, prefix_len=0, suffix_len=0):
        self.index = index
        self.prefix_len = prefix_len
        self.suffix_len = suffix_len

    def __len__(self):
        return len(self.index)

    def __getitem__(self, word):
        word = word.lower()
        if self.prefix_len != 0:
            word = word[:self.prefix_len]
        if self.suffix_len != 0:
            word = word[-self.suffix_len:]
        e = self.index.get(word)
        if e is not None:
            e = e + 1
        return e


def to_eflomal_text_file(sentencefile, outfile, prefix_len=0, suffix_len=0):
    """Write sentences to a file read by eflomal binary

    Arguments:

    sentencefile - input text file object
    outfile - output file object
    prefix_len - prefix length to remove
    suffix_len - suffix length to remove

    Returns TextIndex object.

    """
    sents, index = read_text(sentencefile, True, prefix_len, suffix_len)
    n_sents = len(sents)
    voc_size = len(index)
    write_text(outfile, tuple(sents), voc_size)
    return TextIndex(index, prefix_len, suffix_len), n_sents, voc_size


def sentences_from_joint_file(joint_file, index=None):
    """Yield sentences from joint sentences file"""
    for i, line in enumerate(joint_file):
        fields = line.strip().split(' ||| ')
        if len(fields) != 2:
            logger.error('line %d does not contain a single |||'
                         ' separator, or sentence(s) are empty!',
                         i + 1)
            raise ValueError('Invalid joint input line %s' % line)
        if index is None:
            yield fields[0], fields[1]
        else:
            yield fields[index]


def calculate_priors(src_sentences, trg_sentences,
                     fwd_alignments, rev_alignments):
    """Calculate priors from alignments"""
    priors = Counter()
    hmmf_priors = Counter()
    hmmr_priors = Counter()
    ferf_priors = Counter()
    ferr_priors = Counter()
    for lineno, (src_sent, trg_sent, fwd_line, rev_line) in enumerate(
            zip(src_sentences, trg_sentences, fwd_alignments, rev_alignments)):
        src_sent = src_sent.strip().split()
        trg_sent = trg_sent.strip().split()
        fwd_links = [tuple(map(int, s.split('-'))) for s in fwd_line.split()]
        rev_links = [tuple(map(int, s.split('-'))) for s in rev_line.split()]
        for i, j in fwd_links:
            if i >= len(src_sent) or j >= len(trg_sent):
                logger.error('alignment out of bounds in line %d: '
                             '(%d, %d)', lineno + 1, i, j)
                raise ValueError('Invalid input on line %d' % lineno + 1)
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
    return priors, hmmf_priors, hmmr_priors, ferf_priors, ferr_priors


def write_priors(priorsf, priors_list, hmmf_priors, hmmr_priors,
                 ferf_priors, ferr_priors):
    """Write priors to file object"""
    for (src, trg), alpha in sorted(priors_list.items()):
        print('LEX\t%s\t%s\t%g' % (src, trg, alpha), file=priorsf)
    for (src, fert), alpha in sorted(ferf_priors.items()):
        print('FERF\t%s\t%d\t%g' % (src, fert, alpha), file=priorsf)
    for (trg, fert), alpha in sorted(ferr_priors.items()):
        print('FERR\t%s\t%d\t%g' % (trg, fert, alpha), file=priorsf)
    for jump, alpha in sorted(hmmf_priors.items()):
        print('HMMF\t%d\t%g' % (jump, alpha), file=priorsf)
    for jump, alpha in sorted(hmmr_priors.items()):
        print('HMMR\t%d\t%g' % (jump, alpha), file=priorsf)


def read_priors(priors_file):
    """Load priors from file object"""
    priors_list = []    # list of (srcword, trgword, alpha)
    ferf_priors = []    # list of (wordform, alpha)
    ferr_priors = []    # list of (wordform, alpha)
    hmmf_priors = {}    # dict of jump: alpha
    hmmr_priors = {}    # dict of jump: alpha
    # 5 types of lines valid:
    #
    # LEX   srcword     trgword     alpha   | lexical prior
    # HMMF  jump        alpha               | target-side HMM prior
    # HMMR  jump        alpha               | source-side HMM prior
    # FERF  srcword     fert   alpha        | source-side fertility p.
    # FERR  trgword     fert    alpha       | target-side fertility p.
    for i, line in enumerate(priors_file):
        fields = line.rstrip('\n').split('\t')
        try:
            alpha = float(fields[-1])
        except ValueError as err:
            logger.error('priors line %d contains alpha '
                         'value of "%s" which is not numeric',
                         i+1, fields[2])
            raise err
        if fields[0] == 'LEX' and len(fields) == 4:
            priors_list.append((fields[1], fields[2], alpha))
        elif fields[0] == 'HMMF' and len(fields) == 3:
            hmmf_priors[int(fields[1])] = alpha
        elif fields[0] == 'HMMR' and len(fields) == 3:
            hmmr_priors[int(fields[1])] = alpha
        elif fields[0] == 'FERF' and len(fields) == 4:
            ferf_priors.append((fields[1], int(fields[2]), alpha))
        elif fields[0] == 'FERR' and len(fields) == 4:
            ferr_priors.append((fields[1], int(fields[2]), alpha))
        else:
            logger.error('priors line %d is invalid', i + 1)
            raise ValueError('Invalid input on line %d' % i + 1)
    return priors_list, hmmf_priors, hmmr_priors, ferf_priors, ferr_priors


def to_eflomal_priors_file(priors, src_index, trg_index, outfile):
    """Write priors to a file read by eflomal binary

    Arguments:

    priors - tuple of priors (priors_list, hmmf_priors, hmmr_priors,
             ferf_priors, ferr_priors)
    src_index - vocabulary index for source text
    tgt_index - vocabulary index for target text
    outfile - file object for output

    """
    priors_list, hmmf_priors, hmmr_priors, ferf_priors, ferr_priors = priors
    priors_indexed = {}
    for src_word, trg_word, alpha in priors_list:
        if src_word == '<NULL>':
            e = 0
        else:
            e = src_index[src_word]

        if trg_word == '<NULL>':
            f = 0
        else:
            f = trg_index[trg_word]

        if (e is not None) and (f is not None):
            priors_indexed[(e, f)] = priors_indexed.get((e, f), 0.0) \
                + alpha
    ferf_indexed = {}
    for src_word, fert, alpha in ferf_priors:
        e = src_index[src_word]
        if e is not None:
            ferf_indexed[(e, fert)] = \
                ferf_indexed.get((e, fert), 0.0) + alpha
    ferr_indexed = {}
    for trg_word, fert, alpha in ferr_priors:
        f = trg_index[trg_word]
        if f is not None:
            ferr_indexed[(f, fert)] = \
                ferr_indexed.get((f, fert), 0.0) + alpha
    logger.info('%d (of %d) pairs of lexical priors used',
                len(priors_indexed), len(priors_list))
    print('%d %d %d %d %d %d %d' % (
        len(src_index)+1, len(trg_index)+1, len(priors_indexed),
        len(hmmf_priors), len(hmmr_priors),
        len(ferf_indexed), len(ferr_indexed)),
          file=outfile)
    for (e, f), alpha in sorted(priors_indexed.items()):
        print('%d %d %g' % (e, f, alpha), file=outfile)
    for jump, alpha in sorted(hmmf_priors.items()):
        print('%d %g' % (jump, alpha), file=outfile)
    for jump, alpha in sorted(hmmr_priors.items()):
        print('%d %g' % (jump, alpha), file=outfile)
    for (e, fert), alpha in sorted(ferf_indexed.items()):
        print('%d %d %g' % (e, fert, alpha), file=outfile)
    for (f, fert), alpha in sorted(ferr_indexed.items()):
        print('%d %d %g' % (f, fert, alpha), file=outfile)
    outfile.flush()
