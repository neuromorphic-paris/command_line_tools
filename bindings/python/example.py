import ctypes
import os
import sys

print(os.path.join(
    os.path.dirname(os.path.realpath(__file__)),
    'build',
    'release',
    'libeventstream.so'))

extension = '.so'
if sys.platform == "darwin":
    extension = '.dylib'
eventstream = ctypes.cdll.LoadLibrary(os.path.join(
    os.path.dirname(os.path.realpath(__file__)),
    'build',
    'release',
    'libeventstream.so'))

eventstream.read()
