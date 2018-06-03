![utilities](banner.png "The Utilities banner")

Utilities bundles several command-line applications and scripts to manipulate event files.

# Install

To download Utilities, run the command:
```sh
git clone --recursive https://github.com/neuromorphic-paris/utilities.git
```

## Applications

The provided applications rely on [Premake 4.x](https://github.com/premake/premake-4.x) (x ≥ 3) to generate build configurations. Follow these steps to install it:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt-get install premake4`.
  - __OS X__: Open a terminal and execute the command `brew install premake`. If the command is not found, you need to install Homebrew first with the command<br />
    `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

Run the following commands from the *utilities* directory to compile the applications:
```
premake4 gmake
cd build
make
cd release
```

The command-line applications are located in the *release* directory.

## Scripts

The scripts use [Python 3](https://www.python.org). Follow these steps to install it:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt-get install python3`.
  - __OS X__: Open a terminal and execute the command `brew install python3`. If the command is not found, you need to install Homebrew first with the command<br />
    `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

# Documentation

## Applications

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
./dat_to_es [options] /path/to/input_td.dat /path/to/output.es
```
Available options:
  - `-a [/path/to/input_aps.dat]`, `--aps [/path/to/input_aps.dat]` merges `input_td` and `input_aps` into a single ATIS Event Stream file. If this option is not used, a DVS Event Stream file is generated.
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

## Scripts

### run_for_each.py

run_for_each.py runs a command on each find matching a glob pattern, and writes the outputs to a single directory:
```
python3 run_for_each.py command input_glob output_directory suffix
```
Example:
```
python3 run_for_each.py ../build/release/es_to_csv '/Users/Bob/event_streams/*.es' '/Users/Bob/csvs' .csv
```

# Contribute

## Development dependencies

[ClangFormat](https://clang.llvm.org/docs/ClangFormat.html) is used to unify coding styles. Follow these steps to install it:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt-get install clang-format`.
  - __OS X__: Open a terminal and execute the command `brew install clang-format`. If the command is not found, you need to install Homebrew first with the command<br />
    `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

## Test

To test the library, run from the *utilities* directory:
```sh
premake4 gmake
cd build
make
cd release
```

You can then run sequentially the executables located in the *release* directory.

After changing the code, format the source files by running from the *utilities* directory:
```sh
for file in source/*.hpp; do clang-format -i $file; done;
for file in source/*.cpp; do clang-format -i $file; done;
for file in scripts/*.cpp; do clang-format -i $file; done;
```

# License

See the [LICENSE](LICENSE.txt) file for license rights and limitations (GNU GPLv3).
