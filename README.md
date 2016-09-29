![utilities](utilitiesBanner.png "The Utilities banner")

# Utilities

Utilities bundles several applications to manipulate event files.

# Installation

## Dependencies

Utilities relies on [Premake 4.x](https://github.com/premake/premake-4.x) (x ≥ 3), which is a utility to ease the install process. Follow these steps:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt-get install premake4`.
  - __Fedora__: Open a terminal and execute the command `sudo dnf install premake`. Then, run<br />
  `echo '/usr/local/lib' | sudo tee /etc/ld.so.conf.d/neuromorphic-paris.conf > /dev/null`.
  - __OS X__: Open a terminal and execute the command `brew install premake`. If the command is not found, you need to install Homebrew first with the command<br />
  `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

Utilities also depends on three home-made libraries Sepia, Tarsier and Pontella. They can be installed from source by executing the following commands from a terminal:
```sh
git clone git@github.com:neuromorphic-paris/sepia.git
cd sepia && premake4 install && cd .. && rm -rf sepia
git clone git@github.com:neuromorphic-paris/tarsier.git
cd tarsier && premake4 install && cd .. && rm -rf tarsier
git clone git@github.com:neuromorphic-paris/pontella.git
cd pontella && premake4 install && cd .. && rm -rf pontella
```

## Build

To build the applications, go to the *utilities* directory and run `premake4 gmake && cd build && make`.
The applications are compiled as command line executables in the *build/Release* folder.

# Documentation

## Rainmaker

Rainmaker generates a standalone html file with a 3D representation of events from an oka file. The syntax is as follows:
```sh
./rainmaker [options] /path/to/input.oka /path/to/output.html\n"
```
Available options are:\n"
  - `-t [timestamp], --timestamp [timestamp]` sets the initial timestamp for the point cloud (defaults to `0`)
  - `-d [duration], --duration [duration]` sets the duration (in microseconds) for the point cloud (defaults to `1000000`)
  - `-e [decay], --decay [decay]` sets the decay used by the maskIsolated handler (defaults to `10000`)
  - `-r [ratio], --ratio [ratio]` sets the discard ratio for logarithmic tone mapping (default to `0.05`)
  - `-f [frame time], --frametime [frame time]` sets the time between two frames (defaults to `auto`), `auto` calculates the time between two frames so that there is the same amount of raw data in events and frames, a duration in microseconds can be provided instead, `none` disables the frames
  - `-c, --change` displays change detections instead of exposure measurements, the ratio and frame time options are ignored
  - `-h, --help` shows the help message

## OkaToCsv

OkaToCsv converts an oka file into a csv file (compatible with Excel and Matlab). The syntax is as follows:
```sh
./okaToCsv [options] /path/to/input.oka /path/to/output.csv
```
Available options are:
  - `-h, --help` shows the help message

## DatToOka

DatToOka converts a td file and an aps file into an oka file. The syntax is as follows:
```sh
./datToOka [options] /path/to/input_td.dat /path/to/input_aps.dat /path/to/output.oka
```
If the charcaters chain 'null' (without quotes) is given for the td / aps file, the oka file is build from the aps / td file only.
Available options are:
  - `-h, --help` shows the help message

# License

See the [LICENSE](LICENSE.md) file for license rights and limitations (MIT).
