"""Unit tests for eflomal"""

import io
import os
import tempfile
import unittest

import eflomal


DATADIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'testdata')


class TestAlign(unittest.TestCase):

    def setUp(self):
        with open(os.path.join(DATADIR, 'test1.sv'), 'r', encoding='utf-8') as fobj:
            self.src_data = fobj.readlines()
        with open(os.path.join(DATADIR, 'test1.en'), 'r', encoding='utf-8') as fobj:
            self.trg_data = fobj.readlines()
        with open(os.path.join(DATADIR, 'test1.priors'), 'r', encoding='utf-8') as fobj:
            self.priors_data = fobj.readlines()

    def test_aligner(self):
        """Test aligner"""
        aligner = eflomal.Aligner()
        with tempfile.NamedTemporaryFile('w+') as fwd_links, \
             tempfile.NamedTemporaryFile('w+') as rev_links:
            aligner.align(self.src_data, self.trg_data,
                          links_filename_fwd=fwd_links.name, links_filename_rev=rev_links.name,
                          quiet=False)
            fwd_links.seek(0)
            self.assertEqual(len(fwd_links.readlines()), 3)
            rev_links.seek(0)
            self.assertEqual(len(rev_links.readlines()), 3)

    def test_aligner_with_priors(self):
        """Test aligner with priors"""
        aligner = eflomal.Aligner()
        with tempfile.NamedTemporaryFile('w+') as fwd_links, \
             tempfile.NamedTemporaryFile('w+') as rev_links:
            aligner.align(self.src_data, self.trg_data,
                          links_filename_fwd=fwd_links.name, links_filename_rev=rev_links.name,
                          priors_input=self.priors_data, quiet=False)
            fwd_links.seek(0)
            self.assertEqual(len(fwd_links.readlines()), 3)
            rev_links.seek(0)
            self.assertEqual(len(rev_links.readlines()), 3)

    def test_makepriors(self):
        """Test creating priors"""
        aligner = eflomal.Aligner()
        with tempfile.NamedTemporaryFile('w+') as fwd_links, \
             tempfile.NamedTemporaryFile('w+') as rev_links:
            aligner.align(self.src_data, self.trg_data,
                          links_filename_fwd=fwd_links.name, links_filename_rev=rev_links.name,
                          quiet=False)
            fwd_links.seek(0)
            rev_links.seek(0)
            priors_tuple = eflomal.calculate_priors(
                self.src_data, self.trg_data, fwd_links.readlines(),
                rev_links.readlines(), False)
            self.assertEqual(len(priors_tuple), 5)
            for prior_list in priors_tuple:
                self.assertGreater(len(prior_list), 0)
        with io.StringIO() as priorsf:
            eflomal.write_priors(priorsf, *priors_tuple)
            priorsf.seek(0)
            self.assertGreater(len(priorsf.readlines()), 5)
