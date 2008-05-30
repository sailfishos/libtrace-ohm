#!/usr/bin/python

from distutils.core import setup, Extension

_nsntrace = Extension('_nsntrace',
                     define_macros = [('MAJOR_VERSION', '0'),
                                      ('MINOR_VERSION', '0')],
                     include_dirs = ['../include'],
                     libraries = ['trace'],
                     library_dirs = ['../lib'],
                     extra_compile_args = ['-O0', '-g3'],
                     sources = ['nsntrace.c'])

setup(name        = '_nsntrace',
      version     = '0.0',
      description = 'NSN trace library python bindings.',
      ext_modules = [_nsntrace])
