#!/usr/bin/env python3

from setuptools import setup, Extension
from Cython.Build import cythonize
import numpy

cyalign_ext=Extension('eflomal', ['python/eflomal/eflomal.pyx'],
                      include_dirs=[numpy.get_include()])

with open('README.md', 'r') as fh:
    long_description = fh.read()

setup(
    name='eflomal',
    version='0.1',
    author='Robert Östling',
    url='https://github.com/robertostling/eflomal',
    license='GNU GPLv3',
    description='pip installable eflomal',
    long_description=long_description,
    long_description_content_type='text/markdown',
    install_requires=['numpy', 'Cython'],
    ext_modules=cythonize(cyalign_ext, language_level='3'),
    scripts=['align.py', 'makepriors.py', 'mergefiles.py']
)

