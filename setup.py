#!/usr/bin/env python3

from setuptools import setup, Extension
from setuptools.command.build_py import build_py as _build_py
import subprocess
from Cython.Build import cythonize
import numpy


class build_py(_build_py):
    """Custom handler for the 'build_py' command."""

    def run(self):
        subprocess.check_call('make -C src eflomal', shell=True)
        subprocess.check_call('make -C src python-install', shell=True)
        return super().run()


cyalign_ext=Extension('eflomal.cython', ['python/eflomal/eflomal.pyx'],
                      include_dirs=[numpy.get_include()])

with open('README.md', 'r') as fh:
    long_description = fh.read()

install_requires = ['numpy', 'Cython']
tests_require = ['pytest']

setup(
    name='eflomal',
    version='2.0.0',
    author='Robert Östling',
    url='https://github.com/robertostling/eflomal',
    license='GNU GPLv3',
    description='pip installable eflomal',
    long_description=long_description,
    long_description_content_type='text/markdown',
    install_requires=install_requires,
    tests_require=tests_require,
    extras_require={'test': tests_require},
    packages=['eflomal'],
    package_dir={'': 'python'},
    package_data={
        'eflomal': ['bin/eflomal']
    },
    ext_modules=cythonize(cyalign_ext, language_level='3'),
    scripts=['python/scripts/eflomal-align', 'python/scripts/eflomal-makepriors'],
    cmdclass={'build_py': build_py}
)
