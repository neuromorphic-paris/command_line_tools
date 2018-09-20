import distutils.core
import numpy
import sys

libraries = []
if sys.platform == 'linux':
    libraries.append('pthread')

eventstream = distutils.core.Extension(
    'eventstream',
    language='c++',
    sources=['eventstream.cpp'],
    extra_compile_args=['-std=c++11'],
    extra_link_args=['-std=c++11'],
    include_dirs=[numpy.get_include()],
    libraries=libraries)
distutils.core.setup(ext_modules=[eventstream])
