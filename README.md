![banner](banner.png)

Command-line tools bundles command-line applications to manipulate event files.

- [install](#install)
    - [clone](#clone)
    - [dependencies](#dependencies)
        - [Debian / Ubuntu](#debian--ubuntu)
        - [macOS](#macos)
        - [Windows](#windows)
    - [compilation](#compilation)
- [documentation](#documentation)
    - [timecode](#timecode)
    - [crop](#crop)
    - [cut](#cut)
    - [dat\_to\_es](#dat_to_es)
    - [es\_to\_csv](#es_to_csv)
    - [es\_to\_frames](#es_to_frames)
    - [es\_to\_ply](#es_to_ply)
    - [event\_rate](#event_rate)
    - [evt3\_to\_es](#evt3_to_es)
    - [rainmaker](#rainmaker)
    - [rainbow](#rainbow)
    - [size](#size)
    - [spectrogram](#spectrogram)
    - [spatiospectrogram](#spatiospectrogram)
    - [statistics](#statistics)
- [contribute](#contribute)
    - [development dependencies](#development-dependencies)
        - [Debian / Ubuntu](#debian--ubuntu-1)
        - [macOS](#macos-1)
    - [test](#test)
- [license](#license)

# install

## clone

To download Command Line Tools, run:

```sh
git clone --recursive https://github.com/neuromorphic-paris/command_line_tools.git
```

## dependencies

### Debian / Ubuntu

Open a terminal and run:

```sh
sudo apt install premake4
```

### macOS

Open a terminal and run:

```sh
brew tap tonyseek/premake
brew install tonyseek/premake/premake4
```

If the command is not found, you need to install Homebrew first with the command:

```sh
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

### Windows

Install Chocolatey (https://chocolatey.org/) by running as administrator (Start > Windows Powershell > Right click > Run as Administator):

```sh
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
```

Open Powershell as administator and run:

```sh
choco install -y premake.portable
choco install -y visualstudio2019buildtools
choco install -y visualstudio2019-workload-vctools
choco install -y ffmpeg
```

## compilation

Run the following commands from the _command_line_tools_ directory to compile the applications:

```sh
premake4 gmake
cd build
make
cd release
```

**Windows** users must run the following commands from non-administrator Powershell instead:

```sh
premake4 vs2010
cd build
Get-ChildItem . -Filter *.vcxproj | Foreach-Object {C:\Program` Files` `(x86`)\Microsoft` Visual` Studio\2019\BuildTools\MSBuild\Current\Bin\MsBuild.exe /p:PlatformToolset=v142 /property:Configuration=Release $_}
```

The command-line applications are located in the _release_ directory.

# documentation

## timecode

Time parameters in command-line applications (timestamps, durations, decays...) support three input formats:

-   raw integers (`42`, `12345`...), interpreted as microseconds
-   hh:mm:ss timecodes (`00:00:00`, `04:20:00`, `0:0:678`, `00:1440:00`...)
-   hh:mm:ss.uuuuuu timecodes (`00:00:00.123456`, `04:20:00.000000`, `0:0:678.9`...), timecodes with more than 6 digits are rounded to the nearest microsecond.

## crop

crop generates a new Event Stream file with only events from the given region.

```sh
./crop [options] /path/to/input.es /path/to/output.es left bottom width height
```

Available options:

-   `-p`, `--preserve-offset` prevents the coordinates of the cropped area from being normalized
-   `-h`, `--help` shows the help message

## cut

cut generates a new Event Stream file with only events from the given time range.

```sh
./cut [options] /path/to/input.es /path/to/output.es begin end
```

`begin` and `end` must be timecodes.

Available options:

-   `-h`, `--help` shows the help message

## dat_to_es

dat_to_es converts a TD file (and an optional APS file) to an Event Stream file.

```sh
./dat_to_es [options] /path/to/input_td.dat /path/to/input_aps.dat /path/to/output.es
```

If the string `none` is used for the td (respectively, aps) file, the Event Stream file is build from the aps (respectively, td) file only.

Available options:

-   `-h`, `--help` shows the help message

## es_to_csv

es_to_csv converts an Event Stream file to a CSV file (compatible with Excel and Matlab):

```sh
./es_to_csv [options] /path/to/input.es /path/to/output.csv
```

Available options:

-   `-h`, `--help` shows the help message

## es_to_frames

es_to_frames converts an Event Stream file to video frames. Frames use the P6 Netpbm format (https://en.wikipedia.org/wiki/Netpbm) if the output is a directory. Otherwise, the output consists of raw rgb24 frames.

```sh
./es_to_frames [options]
```

Available options:

-   `-i file`, `--input file` sets the path to the input .es file (defaults to standard input)
-   `-o directory`, `--output directory` sets the path to the output directory (defaults to standard output)
-   `-b timestamp`, `--begin timestamp` ignores events before this timestamp (timecode, defaults to `00:00:00`),
-   `-e timestamp`, `--end timestamp` ignores events after this timestamp (timecode, defaults to the end of the recording),
-   `-f frametime`, `--frametime frametime` sets the time between two frames (timecode, defaults to `00:00:00.020`)
-   `-s style`, `--style style` selects the decay function, one of `exponential` (default), `linear`, `window`, `cumulative`, and `cumulative_shared`
-   `-t tau`, `--tau tau` sets the decay function parameter (timecode, defaults to `00:00:00.200`)
    -   if `style` is `exponential`, the decay is set to `parameter`
    -   if `style` is `linear`, the decay is set to `parameter / 2`
    -   if `style` is `window`, the time window is set to `parameter`
    -   if `style` is `cumulative`, the time window is set to `parameter`
    -   if `style` is `cumulative-shared`, the time window is set to `parameter`
-   `-j color`, `--oncolor color` sets the color for ON events (color must be formatted as `#hhhhhh` where `h` is an hexadecimal digit, defaults to `#f4c20d`)
-   `-k color`, `--offcolor color` sets the color for OFF events (color must be formatted as `#hhhhhh` where `h` is an hexadecimal digit, defaults to `#1e88e5`)
-   `-l color`, `--idlecolor color` sets the background color (color must be formatted as `#hhhhhh` where `h` is an hexadecimal digit, defaults to `#191919`)
-   `-r ratio`, `--discard-ratio ratio` sets the ratio of pixels discarded for cumulative mapping, ignored if the style is cumulative or cumulative-shared (defaults to 0.01)
-   `-a`, `--add-timecode` adds a timecode overlay
-   `-d digits`, `--digits digits` sets the number of digits in output filenames, ignored if the output is not a directory (defaults to `6`)
-   `-r ratio`, `--discard-ratio ratio` sets the ratio of pixels discarded for tone mapping, ignored if the stream type is not atis, used for black (resp. white) if `--black` (resp. `--white`) is not set (defaults to 0.01)
-   `-v duration`, `--black duration` sets the black integration duration for tone mapping (timecode, defaults to automatic discard calculation)
-   `-w duration`, `--white duration` sets the white integration duration for tone mapping (timecode, defaults to automatic discard calculation)
-   `-x color`, `--atiscolor color` sets the background color for ATIS exposure measurements (color must be formatted as #hhhhhh where h is an hexadecimal digit, defaults to `#000000`)
-   `-h`, `--help` shows the help message

Once can use the script _render.py_ to directly generate an MP4 video instead of frames. _es_to_frames_ must be compiled before using _render.py_, and FFmpeg (https://www.ffmpeg.org) must be installed and on the system's path. Run `python3 render.py --help` for details.

The commands below show how to manually pipe the generated frames into FFmpeg:

```sh
cat /path/to/input.es | ./es_to_frames | ffmpeg -f rawvideo -s 1280x720 -framerate 50 -pix_fmt rgb24 -i - -c:v libx264 -pix_fmt yuv420p /path/to/output.mp4
```

You may need to change the width, height and framerate of the video depending on the `es_to_frames` options and the Event Stream dimensions. You can use `./size /path/to/input.es` to read the dimensions:

```sh
cat /path/to/input.es | ./es_to_frames --frametime 10000 | ffmpeg -f rawvideo -s $(./size /path/to/input.es) -framerate 100 -pix_fmt rgb24 -i - -c:v libx264 -pix_fmt yuv420p /path/to/output.mp4
```

You can also use a lossless encoding format:

```sh
cat /path/to/input.es | ./es_to_frames | ffmpeg -f rawvideo -s 1280x720 -framerate 50 -pix_fmt rgb24 -i - -c:v libx265 -x265-params lossless=1 -pix_fmt yuv444p /path/to/output.mp4
```

## es_to_ply

es_to_ply converts an Event Stream file to a PLY file (Polygon File Format, compatible with Blender).

```sh
./es_to_ply [options] /path/to/input.es /path/to/output_on.ply /path/to/output_off.ply
```

Available options:

-   `-t timestamp`, `--timestamp timestamp` sets the initial timestamp for the point cloud (timecode, defaults to `00:00:00`)
-   `-d duration`, `--duration duration` sets the duration for the point cloud (timecode, defaults to `00:00:01`)
-   `-h`, `--help` shows the help message

## event_rate

event_rate plots the number of events per second (slidding time window).

```sh
./event_rate [options] /path/to/input.es /path/to/output.svg
```

Available options:

-   `-l tau`, `--long tau` sets the long (foreground curve) time window (timecode, defaults to `00:00:01`)
-   `-s tau`, `--short tau` sets the short (background curve) time window (timecode, defaults to `00:00:00.010`)
-   `-i size`, `--width size` sets the output width in pixels (defaults to `1280`)
-   `-e size`, `--height size` sets the output height in pixels (defaults to `720`)
-   `-h`, `--help` shows the help message

## evt3_to_es

evt3_to_es converts a raw file (EVT3) into an Event Stream file.

```sh
./evt3_to_es [options] /path/to/input.raw /path/to/output.es
```

Available options:

-   `-h`, `--help` shows the help message

## rainmaker

rainmaker generates a standalone HTML file containing a 3D representation of events from an Event Stream file.

```sh
./rainmaker [options] /path/to/input.es /path/to/output.html
```

Available options:

-   `-t timestamp`, `--timestamp timestamp` sets the initial timestamp for the point cloud (timecode, defaults to `00:00:00`)
-   `-d duration`, `--duration duration` sets the duration for the point cloud (timecode, defaults to `00:00:01`)
-   `-r ratio`, `--ratio ratio` sets the discard ratio for logarithmic tone mapping (default to `0.05`, ignored if the file does not contain ATIS events)
-   `-f duration`, `--frametime duration` sets the time between two frames (defaults to `auto`), `auto` calculates the time between two frames so that there is the same amount of raw data in events and frames, a duration in microseconds can be provided instead, `none` disables the frames, ignored if the file contains DVS events
-   `-a`, `--dark` renders in dark mode
-   `-h`, `--help` shows the help message

## rainbow

rainbow represents events by mapping time to colors.

```sh
./rainbow [options] /path/to/input.es /path/to/output.ppm
```

Available options:

-   `-a alpha`, `--alpha alpha` sets the transparency level for each event (must be in the range ]0, 1], defaults to `0.1`)
-   `-l color`, `--idlecolor color` sets the background color (color must be formatted as `#hhhhhh` where `h` is an hexadecimal digit, defaults to `#191919`)
-   `-h`, `--help` shows the help message

## size

size prints the spatial dimensions of the given Event Stream file.

```sh
./size /path/to/input.es
```

Available options:

-   `-h`, `--help` shows the help message

## spectrogram

spectrogram plots a short-time Fourier transform.

```sh
./spectrogram [options] /path/to/input.es /path/to/output.png /path/to/output.json
```

Available options:

-   `-b timecode`, `--begin [timecode]` ignores events before this timestamp (timecode, `defaults to 00:00:00`)
-   `-e [timecode]`, `--end [timecode]` ignores events after this timestamp (timecode, defaults to the end of the recording)
-   `-l [int]`, `--left [int]` input region of interest in pixels (defaults to 0)
-   `-o [int]`, `--bottom [int]` input region of interest in pixels (defaults to 0)
-   `-c [int]`, `--width [int]` input region of interest in pixels (defaults to input width)
-   `-d [int]`, `--height [int]` input region of interest in pixels (defaults to input height)
-   `-t [int]`, `--tau [int]` decay in µs (defaults to `100000`)
-   `-m [mode]`, `--mode [mode]` polarity mode, one of `on`, `off`, `all`, `abs` (defaults to `all`)
    -   `on` only uses ON events
    -   `off` only uses OFF events
    -   `all` multiplies the complex activity by 1 for ON events and -1 for OFF events
    -   `abs` multiplies the complex activity by 1 for all events
-   `-i [float]`, `--minimum [float]` minimum frequency in Hertz (defaults to `5e6 / (end - begin)`)
-   `-j [float]`, `--maximum [float]` maximum frequency in Hertz (defaults to `10000.0`)
-   `-f [int]`, `--frequencies [int]` number of frequencies (defaults to `100`)
-   `-s [int]`, `--times [int]` number of time samples (defaults to `1000`)
-   `-g [float]`, `--gamma [float]` gamma ramp (power) to apply to the output image (defaults to `0.5`)
-   `-h`, `--help` shows the help message

## spatiospectrogram

-   `-i [path]`, `--input [path]` sets the path to the input .es file (defaults to standard input)
-   `-o [path]`, `--output [path]` sets the path to the output directory (defaults to standard output)
-   `-b [timecode]`, `--begin [timecode]` ignores events before this timestamp (timecode, defaults to `00:00:00`)
-   `-e [timecode]`, `--end [timecode]` ignores events after this timestamp (timecode, defaults to the end of the recording)
-   `-f [timecode]`, `--frametime [timecode]` sets the time between two frames (timecode, defaults to `00:00:00.020`)
-   `-c [int]`, `--scale [int]` scale up the output by the given integer factor (defaults to `1`)
-   `-t [int]`, `--tau [int]` decay in µs (defaults to `100000`)
-   `-m [mode]`, `--mode [mode]` polarity mode, one of `on`, `off`, `all`, `abs` (defaults to `all`)
    -   `on` only uses ON events
    -   `off` only uses OFF events
    -   `all` multiplies the complex activity by 1 for ON events and -1 for OFF events
    -   `abs` multiplies the complex activity by 1 for all events
-   `-p [float]`, `--minimum [float]` minimum frequency in Hertz (defaults `10.0`)
-   `-q [float]`, `--maximum [float]` maximum frequency in Hertz (defaults to `10000.0`)
-   `-u [int]`, `--frequencies [int]` number of frequencies (defaults to `100`)
-   `-g [float]`, `--frequency-gamma [float]` gamma ramp (power) to apply to the output frequency (defaults to `0.5`)
-   `-k [float]`, `--amplitude-gamma [float]` gamma ramp (power) to apply to the output amplitude (defaults to `0.5`)
-   `-r [float]`, `--discard [float]` amplitude discard ratio for tone-mapping (defaults to `0.001`)
-   `-a`, `--add-timecode` adds a timecode overlay
-   `-d [int]`, `--digits [int]` sets the number of digits in output filenames, ignored if the output is not a directory (defaults to `6`)
-   `-h`, `--help` shows the help message

The commands below show how to manually pipe the generated frames into FFmpeg:

```sh
cat /path/to/input.es | ./spatiospectrogram | ffmpeg -f rawvideo -s 1280x720 -framerate 50 -pix_fmt rgb24 -i - -c:v libx264 -pix_fmt yuv420p /path/to/output.mp4
```

You may need to change the width, height and framerate of the video depending on the `spatiospectrogram` options and the Event Stream dimensions. You can use `./size /path/to/input.es` to read the dimensions:

```sh
cat /path/to/input.es | ./spatiospectrogram --frametime 10000 | ffmpeg -f rawvideo -s $(./size /path/to/input.es) -framerate 100 -pix_fmt rgb24 -i - -c:v libx264 -pix_fmt yuv420p /path/to/output.mp4
```

You can also use a lossless encoding format:

```sh
cat /path/to/input.es | ./spatiospectrogram | ffmpeg -f rawvideo -s 1280x720 -framerate 50 -pix_fmt rgb24 -i - -c:v libx265 -x265-params lossless=1 -pix_fmt yuv444p /path/to/output.mp4
```

## statistics

statistics retrieves the event stream's properties and outputs them in JSON format.

```sh
./statistics [options] /path/to/input.es
```

Available options:

-   `-h`, `--help` shows the help message

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

To test the library, run from the _command_line_tools_ directory:

```sh
premake4 gmake
cd build
make
cd release
```

**Windows** users must run `premake4 vs2010` instead, and open the generated solution with Visual Studio.

You can then run sequentially the executables located in the _release_ directory.

After changing the code, format the source files by running from the _command_line_tools_ directory:

```sh
clang-format -i source/*.hpp source/*.cpp
```

**Windows** users must run _Edit_ > _Advanced_ > _Format Document_ from the Visual Studio menu instead.

# license

See the [LICENSE](LICENSE.txt) file for license rights and limitations (GNU GPLv3).
