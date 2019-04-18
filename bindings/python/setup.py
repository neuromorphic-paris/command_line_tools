import distutils.core
import numpy
import sys

include_dirs=[numpy.get_include()]
libraries = []
if sys.platform == 'linux':
    libraries.append('')
    libraries.append('pthread')

eventstream = distutils.core.Extension(
    'eventstream',
    language='c++',
    sources=['eventstream.cpp'],
    extra_compile_args=['-std=c++11'],
    extra_link_args=['-std=c++11'],
    include_dirs=include_dirs,
    libraries=libraries)
distutils.core.setup(ext_modules=[eventstream])
