![utilities](banner.png "The Utilities banner")

Utilities bundles several command-line applications and scripts to manipulate event files.

# install

## clone

To download Utilities, run the command:
```sh
git clone --recursive https://github.com/neuromorphic-paris/utilities.git
```

## bindings

The bindings provided by utilities are tools to read and write Event Stream files from python and matlab.

### python

#### dependencies

##### Debian / Ubuntu

Open a terminal and run:
```sh
sudo apt install python3 # or 'sudo apt install python' for python 2.x
pip3 install numpy # or 'pip install numpy' for python 2.x
```

##### macOS

Open a terminal and run:
```sh
brew install python3 # or 'brew install python' for python 2.x
pip3 install numpy # or 'pip install numpy' for python 2.x
```

##### Windows

Download and install [python](https://www.python.org). Then, open a terminal and run:
```sh
pip3 install numpy # or 'pip install numpy' for python 2.x
```

#### installation

Open a terminal and run:
```sh
cd bindings/python
python3 setup.py install # or 'python setup.py install' for python 2.x
```

#### documentation

After installing the bindings, you can import the `eventstream` library system-wide. It provides the functions `read` and `write`:
```py
import eventstream

events = eventstream.read('/path/to/input.es')
eventstream.write(events, '/path/to/output.es')
```
The `events` object returned by `read` and expected by `write` is a python dictionary, with one of the following set of fields (depending on the event type):
```py
{
    'type': 'generic', # u'generic' with python 2.x
    't': numpy.array([...], dtype=numpy.uint64),
    'bytes': numpy.array([...], dtype=numpy.object_),
}
```
The objects associated with the key `bytes` must be byte strings (as an example, `b'abc'`).

```py
{
    'type': 'dvs', # u'dvs' with python 2.x
    'width': 320, # 320L with python 2.x
    'height': 240, # 240L with python 2.x
    't': numpy.array([...], dtype=numpy.uint64),
    'x': numpy.array([...], dtype=numpy.uint16),
    'y': numpy.array([...], dtype=numpy.uint16),
    'is_increase': numpy.array([...], dtype=numpy.bool),
}
```
```py
{
    'type': 'atis', # u'atis' with python 2.x
    'width': 320, # 320L with python 2.x
    'height': 240, # 240L with python 2.x
    't': numpy.array([...], dtype=numpy.uint64),
    'x': numpy.array([...], dtype=numpy.uint16),
    'y': numpy.array([...], dtype=numpy.uint16),
    'is_threshold_crossing': numpy.array([...], dtype=numpy.bool),
    'polarity': numpy.array([...], dtype=numpy.bool),
}
```
```py
{
    'type': 'color', # u'color' with python 2.x
    'width': 320, # 320L with python 2.x
    'height': 240, # 240L with python 2.x
    't': numpy.array([...], dtype=numpy.uint64),
    'x': numpy.array([...], dtype=numpy.uint16),
    'y': numpy.array([...], dtype=numpy.uint16),
    'r': numpy.array([...], dtype=numpy.uint8),
    'g': numpy.array([...], dtype=numpy.uint8),
    'b': numpy.array([...], dtype=numpy.uint8),
}
```

## applications

The applications provided by utilities are c++ programs operating on Event Stream files.

### dependencies

#### Debian / Ubuntu

Open a terminal and run:
```sh
sudo apt install premake4 # cross-platform build configuration
```

#### macOS

Open a terminal and run:
```sh
brew install premake # cross-platform build configuration
```
If the command is not found, you need to install Homebrew first with the command:
```sh
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

#### Windows

Download and install:
- [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/). Select at least __Desktop development with C++__ when asked.
- [git](https://git-scm.com)
- [premake 4.x](https://premake.github.io/download.html). In order to use it from the command line, the *premake4.exe* executable must be copied to a directory in your path. After downloading and decompressing *premake-4.4-beta5-windows.zip*, run from the command line:
```sh
copy "%userprofile%\Downloads\premake-4.4-beta5-windows\premake4.exe" "%userprofile%\AppData\Local\Microsoft\WindowsApps"
```

### compilation

Run the following commands from the *utilities* directory to compile the applications:
```
premake4 gmake
cd build
make
cd release
```

__Windows__ users must run `premake4 vs2010` instead, and open the generated solution with Visual Studio.

The command-line applications are located in the *release* directory.

### documentation

#### cut

cut generates a new Event Stream file with only events from the given time range.
```
./cut [options] /path/to/input.es /path/to/output.es begin duration
```
Available options:
  - `-h`, `--help` shows the help message

#### dat_to_es

dat_to_es converts a TD file (and an optional APS file) to an Event Stream file:
```
./dat_to_es [options] /path/to/input_td.dat /path/to/input_aps.dat /path/to/output.es
```
If the string `none` is used for the td (respectively, aps) file, the Event Stream file is build from the aps (respectively, td) file only.
Available options:
  - `-h`, `--help` shows the help message

#### es_to_csv

es_to_csv converts an Event Stream file to a CSV file (compatible with Excel and Matlab):
```
./es_to_csv [options] /path/to/input.es /path/to/output.csv
```
Available options:
  - `-h`, `--help` shows the help message

#### rainmaker

rainmaker generates a standalone HTML file containing a 3D representation of events from an Event Stream file:
```
./rainmaker [options] /path/to/input.es /path/to/output.html
```
Available options:
  - `-t [timestamp]`, `--timestamp [timestamp]` sets the initial timestamp for the point cloud (defaults to `0`)
  - `-d [duration]`, `--duration [duration]` sets the duration (in microseconds) for the point cloud (defaults to `1000000`)
  - `-r [ratio]`, `--ratio [ratio]` sets the discard ratio for logarithmic tone mapping (default to `0.05`, ignored if the file does not contain ATIS events)
  - `-f [duration]`, `--frametime [duration]` sets the time between two frames (defaults to `auto`), `auto` calculates the time between two frames so that there is the same amount of raw data in events and frames, a duration in microseconds can be provided instead, `none` disables the frames, ignored if the file contains DVS events
  - `-h`, `--help` shows the help message

#### statistics

statistics retrieves the event stream's properties and outputs them in JSON format:
```
./statistics [options] /path/to/input.es
```
Available options:
  - `-h`, `--help` shows the help message

# contribute

## development dependencies

### Debian / Ubuntu

Open a terminal and run:
```sh
sudo apt install clang-format # formatting tool
```

### macOS

Open a terminal and run:
```sh
brew install clang-format # formatting tool
```

## test

To test the library, run from the *utilities* directory:
```sh
premake4 gmake
cd build
make
cd release
```

__Windows__ users must run `premake4 vs2010` instead, and open the generated solution with Visual Studio.

You can then run sequentially the executables located in the *release* directory.

After changing the code, format the source files by running from the *utilities* directory:
```sh
for file in source/*.hpp; do clang-format -i $file; done;
for file in source/*.cpp; do clang-format -i $file; done;
for file in scripts/*.cpp; do clang-format -i $file; done;
```

__Windows__ users must run *Edit* > *Advanced* > *Format Document* from the Visual Studio menu instead.

# license

See the [LICENSE](LICENSE.txt) file for license rights and limitations (GNU GPLv3).
