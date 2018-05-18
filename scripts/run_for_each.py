import argparse
import glob
import multiprocessing
import os
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('command', help='command to run for each file')
parser.add_argument('input_glob', help='pattern listing the input files')
parser.add_argument('output_directory', help='target output directory')
parser.add_argument('suffix', help='suffix to add to each file (the suffix must inlude the extension)')
args = parser.parse_args()

if not os.path.exists(args.output_directory):
    os.makedirs(args.output_directory)

def run(filename):
    output_filename = os.path.join(
        args.output_directory,
        os.path.splitext(os.path.basename(filename))[0] + args.suffix)
    if subprocess.call([args.command, filename, output_filename]) == 0:
        print(filename + ' -> ' + output_filename)
    else:
        print('non-zero exit for \'' + ' '.join([args.command, filename, output_filename]) + '\'')

multiprocessing.Pool().map(run, glob.glob(args.input_glob))
