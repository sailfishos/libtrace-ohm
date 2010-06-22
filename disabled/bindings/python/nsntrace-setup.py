##############################################################################
#  This file is part of libtrace                                             #
#                                                                            #
#  Copyright (C) 2010 Nokia Corporation.                                     #
#                                                                            #
#  This library is free software; you can redistribute                       #
#  it and/or modify it under the terms of the GNU Lesser General Public      #
#  License as published by the Free Software Foundation                      #
#  version 2.1 of the License.                                               #
#                                                                            #
#  This library is distributed in the hope that it will be useful,           #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of            #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          #
#  Lesser General Public License for more details.                           #
#                                                                            #
#  You should have received a copy of the GNU Lesser General Public          #
#  License along with this library; if not, write to the Free Software       #
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  #
#  USA.                                                                      #
##############################################################################

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
