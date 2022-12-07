#!/usr/bin/env python3

from setuptools import setup, Extension
from setuptools.command.egg_info import egg_info
from setuptools.command.develop import develop
from setuptools.command.install import install
import subprocess
from Cython.Build import cythonize
import numpy


def compile_and_install_binary():
    subprocess.check_call('make -C src eflomal', shell=True)
    subprocess.check_call('make -C src python-install', shell=True)


class CustomInstall(install):
    """Custom handler for the 'install' command."""
    def run(self):
        compile_and_install_binary()
        super().run()


class CustomEggInfo(egg_info):
    """Custom handler for the 'egg_info' command."""
    def run(self):
        compile_and_install_binary()
        super().run()


class CustomDevelop(develop):
    """Custom handler for the 'develop' command."""
    def run(self):
        compile_and_install_binary()
        super().run()


cyalign_ext=Extension('eflomal.cython', ['python/eflomal/eflomal.pyx'],
                      include_dirs=[numpy.get_include()])

with open('README.md', 'r') as fh:
    long_description = fh.read()

install_requires = ['numpy', 'Cython']
tests_require = ['pytest']

setup(
    name='eflomal',
    version='1.0.0-beta',
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
    cmdclass={'install': CustomInstall, 'develop': CustomDevelop, 'egg_info': CustomEggInfo}
)
