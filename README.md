![utilities](banner.png "The Utilities banner")

Utilities bundles several command-line applications and scripts to manipulate event files.

# install

## clone

To download Utilities, run the command:
```sh
git clone --recursive https://github.com/neuromorphic-paris/utilities.git
```

## applications

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

### compile

Run the following commands from the *utilities* directory to compile the applications:
```
premake4 gmake
cd build
make
cd release
```

__Windows__ users must run `premake4 vs2010` instead, and open the generated solution with Visual Studio.

The command-line applications are located in the *release* directory.

## scripts dependencies

### Debian / Ubuntu

Open a terminal and run:
```sh
sudo apt install python3 # programming language
```

### macOS

Open a terminal and run:
```sh
brew install python3 # programming language
```
If the command is not found, you need to install Homebrew first with the command:
```sh
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

### Windows

Download and install [python 3.x](https://www.python.org/downloads/windows/).

# documentation

## applications

### cut

cut generates a new Event Stream file with only events from the given time range.
```
./cut [options] /path/to/input.es /path/to/output.es begin duration
```
Available options:
  - `-h`, `--help` shows the help message

### dat_to_es

dat_to_es converts a TD file (and an optional APS file) to an Event Stream file:
```
./dat_to_es [options] /path/to/input_td.dat /path/to/input_aps.dat /path/to/output.es
```
If the string `none` is used for the td (respectively, aps) file, the Event Stream file is build from the aps (respectively, td) file only.
Available options:
  - `-h`, `--help` shows the help message

### es_to_csv

es_to_csv converts an Event Stream file to a CSV file (compatible with Excel and Matlab):
```
./es_to_csv [options] /path/to/input.es /path/to/output.csv
```
Available options:
  - `-h`, `--help` shows the help message

### rainmaker

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

### statistics

statistics retrieves the event stream's properties and outputs them in JSON format:
```
./statistics [options] /path/to/input.es
```
Available options:
  - `-h`, `--help` shows the help message

## scripts

### run_for_each.py

run_for_each.py runs a command on each find matching a glob pattern, and writes the outputs to a single directory:
```
python3 run_for_each.py command input_glob output_directory suffix
```
Example:
```
python3 run_for_each.py ../build/release/es_to_csv '/Users/Bob/event_streams/*.es' '/Users/Bob/csvs' .csv
```

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
