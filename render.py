import argparse
import atexit
import pathlib
import re
import subprocess
import sys
import typing

dirname = pathlib.Path(__file__).resolve().parent
timecode_pattern = re.compile(r"^(\d+):(\d+):(\d+)(?:\.(\d+))?$")


def timecode(value: str) -> int:
    if value.isdigit():
        return int(value)
    match = timecode_pattern.match(value)
    if match is None:
        raise argparse.ArgumentTypeError(
            "expected an integer or a timecode (12:34:56.789000)"
        )
    result = (
        int(match[1]) * 3600000000 + int(match[2]) * 60000000 + int(match[3]) * 1000000
    )
    if match[4] is not None:
        fraction_string: str = match[4]
        if len(fraction_string) == 6:
            result += int(fraction_string)
        elif len(fraction_string) < 6:
            result += int(fraction_string + "0" * (6 - len(fraction_string)))
        else:
            result += round(float("0." + fraction_string) * 1e6)
    return result


parser = argparse.ArgumentParser(
    description="Generate a frame-based video from an .es file, using es_to_frames and ffmpeg",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
parser.add_argument("input", help="input .es file")
parser.add_argument(
    "--output",
    "-o",
    help="output .mp4 file, calculated from the input file name if not provided",
)
parser.add_argument(
    "--begin",
    "-b",
    type=timecode,
    default=0,
    help="ignore events before this timestamp (timecode)",
)
parser.add_argument(
    "--end", "-e", type=timecode, help="ignore events after this timestamp (timecode)"
)
parser.add_argument(
    "--frametime",
    "-f",
    type=timecode,
    default=20000,
    help="time between two frames in µs",
)
parser.add_argument(
    "--style",
    "-s",
    choices=["exponential", "linear", "window", "cumulative", "cumulative-shared"],
    default="exponential",
    help="decay function",
)
parser.add_argument(
    "--tau", "-t", type=timecode, default=200000, help="decay function parameter in µs"
)
parser.add_argument("--oncolor", "-j", default="#f4c20d", help="color for ON events")
parser.add_argument("--offcolor", "-k", default="#1e88e5", help="color for OFF events")
parser.add_argument("--idlecolor", "-l", default="#292929", help="background color")
parser.add_argument(
    "--cumulative-ratio",
    "-m",
    type=float,
    default=0.01,
    help="ratio of pixels discarded for cumulative mapping (cumulative and cumulative-shared styles only)",
)
parser.add_argument(
    "--lambda-max",
    "-n",
    help="cumulative mapping maximum activity, defaults to automatic discard calculation",
)
parser.add_argument(
    "--add-timecode", "-a", action="store_true", help="timecode overlay"
)
parser.add_argument(
    "--discard-ratio",
    "-r",
    type=float,
    default=0.01,
    help="ratio of pixels discarded for tone mapping (ATIS only)",
)
parser.add_argument(
    "--black",
    "-v",
    help="black integration duration for tone mapping (timecode, ATIS only)",
)
parser.add_argument(
    "--white",
    "-w",
    help="white integration duration for tone mapping (timecode, ATIS only)",
)
parser.add_argument(
    "--atiscolor",
    "-x",
    default="#000000",
    help="background color for ATIS exposure measurements",
)
parser.add_argument(
    "--ffmpeg",
    "-g",
    default="ffmpeg",
    help="FFmpeg executable",
)
parser.add_argument(
    "--h264-crf",
    type=int,
    default=15,
    help="H264 Constant Rate Factor (CRF)",
)
parser.add_argument(
    "--sonify",
    "-y",
    action="store_true",
    help="generate audio with 'synth'",
)
parser.add_argument(
    "--skip-merge",
    action="store_true",
    help="do not merge audio and video",
)
parser.add_argument(
    "--sonify-amplitude-gain",
    type=float,
    default=0.1,
    help="activity to sound amplitude conversion factor",
)
parser.add_argument(
    "--sonify-minimum-frequency",
    type=float,
    default=27.5,
    help="minimum frequency (bottom row) in Hertz",
)
parser.add_argument(
    "--sonify-maximum-frequency",
    type=float,
    default=4186.009,
    help="maximum frequency (top row) in Hertz",
)
parser.add_argument(
    "--sonify-sampling-rate", type=int, default=44100, help="sampling rate in Hertz"
)
parser.add_argument(
    "--sonify-tracker-lambda",
    type=float,
    default=0.1,
    help="row tracker moving mean parameter",
)
parser.add_argument(
    "--sonify-activity-tau", type=float, default=10000, help="row decay parameter in µs"
)

args = parser.parse_args()

input = pathlib.Path(args.input).resolve()
if args.output is None:
    range_string = ""
    if args.begin > 0 or args.end is not None:
        range_string += f"_begin={args.begin}"
    if args.end is not None:
        range_string += f"_end={args.end}"
    if (args.frametime // 1e6) * 1e6 == args.frametime:
        frametime = f"{args.frametime // 1000000}s"
    elif (args.frametime // 1e3) * 1e3 == args.frametime:
        frametime = f"{args.frametime // 1000}ms"
    else:
        frametime = f"{args.frametime}us"
    if (args.tau // 1e6) * 1e6 == args.tau:
        tau = f"{args.tau // 1000000}s"
    elif (args.tau // 1e3) * 1e3 == args.tau:
        tau = f"{args.tau // 1000}ms"
    else:
        tau = f"{args.tau}us"
    sonify_suffix = "_sound" if args.sonify else ""
    output = (
        input.parent
        / f"{input.stem}{range_string}_{args.style}_frametime={frametime}_tau={tau}{sonify_suffix}.mp4"
    )
else:
    output = pathlib.Path(args.output).resolve()
if input == output:
    raise Exception("input and output must be different files")
if not pathlib.Path(args.input).exists():
    sys.stderr.write(f"{input} does not exist")
if not pathlib.Path(args.input).is_file():
    sys.stderr.write(f"{input} is not a file")

width, height = (
    int(value)
    for value in subprocess.run(
        [str(dirname / "build" / "release" / "size"), str(input)],
        check=True,
        capture_output=True,
    ).stdout.split(b"x")
)

es_to_frames_arguments = [
    str(dirname / "build" / "release" / "es_to_frames"),
    f"--input={str(input)}",
    f"--begin={args.begin}",
    f"--frametime={args.frametime}",
    f"--style={args.style}",
    f"--tau={args.tau}",
    f"--oncolor={args.oncolor}",
    f"--offcolor={args.offcolor}",
    f"--idlecolor={args.idlecolor}",
    f"--cumulative-ratio={args.cumulative_ratio}",
    f"--discard-ratio={args.discard_ratio}",
    f"--atiscolor={args.atiscolor}",
]
if args.end is not None:
    es_to_frames_arguments.append(f"--end={args.end}")
if args.lambda_max is not None:
    es_to_frames_arguments.append(f"--lambda-max={args.lambda_max}")
if args.add_timecode:
    es_to_frames_arguments.append("--add-timecode")
if args.white is not None:
    es_to_frames_arguments.append(f"--white={args.white}")
if args.black is not None:
    es_to_frames_arguments.append(f"--black={args.black}")
es_to_frames = subprocess.Popen(
    es_to_frames_arguments,
    stdout=subprocess.PIPE,
)
assert es_to_frames.stdout is not None

ffmpeg = subprocess.Popen(
    [
        args.ffmpeg,
        "-hide_banner",
        "-loglevel",
        "warning",
        "-stats",
        "-f",
        "rawvideo",
        "-s",
        f"{width}x{height}",
        "-framerate",
        "50",
        "-pix_fmt",
        "rgb24",
        "-i",
        "-",
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        "-crf",
        str(args.h264_crf),
        "-f",
        "mp4",
        "-y",
        f"{output}.render",
    ],
    stdin=subprocess.PIPE,
)
assert ffmpeg.stdin is not None

synth: typing.Optional[subprocess.Popen] = None
if args.sonify:
    synth_arguments = [
        str(dirname / "build" / "release" / "synth"),
        str(input),
        f"{output.with_suffix('.wav')}.render",
        "--output-mode=1",
        f"--begin={args.begin}",
        f"--amplitude-gain={args.sonify_amplitude_gain}",
        f"--minimum-frequency={args.sonify_minimum_frequency}",
        f"--maximum-frequency={args.sonify_maximum_frequency}",
        f"--sampling-rate={args.sonify_sampling_rate}",
        f"--tracker-lambda={args.sonify_tracker_lambda}",
        f"--playback-speed={args.frametime / 20000.0}",
        f"--activity-tau={args.sonify_activity_tau}",
    ]
    if args.end is not None:
        synth_arguments.append(f"--end={args.end}")
    synth = subprocess.Popen(
        synth_arguments,
        stdout=subprocess.PIPE,
    )


def cleanup():
    es_to_frames.kill()
    ffmpeg.kill()
    if synth is not None:
        synth.kill()


atexit.register(cleanup)

frame_size = width * height * 3
while True:
    frame = es_to_frames.stdout.read(frame_size)
    if len(frame) != frame_size:
        break
    ffmpeg.stdin.write(frame)

ffmpeg.stdin.close()
es_to_frames.wait()
ffmpeg.wait()

if not args.sonify or args.skip_merge:
    output.unlink(missing_ok=True)
    pathlib.Path(f"{output}.render").rename(output)

if synth is not None:
    assert synth.stdout is not None
    while True:
        line = synth.stdout.readline()
        if len(line) == 0:
            sys.stdout.write(f"\n")
            break
        sys.stdout.write(f"\r{line[:-1].decode()}")
        sys.stdout.flush()
    synth.wait()

    if args.skip_merge:
        output.with_suffix(".wav").unlink(missing_ok=True)
        pathlib.Path(f"{output.with_suffix('.wav')}.render").rename(
            output.with_suffix(".wav")
        )
    else:
        ffmpeg = subprocess.Popen(
            [
                args.ffmpeg,
                "-hide_banner",
                "-loglevel",
                "warning",
                "-stats",
                "-i",
                f"{output}.render",
                "-guess_layout_max",
                "0",
                "-i",
                f"{output.with_suffix('.wav')}.render",
                "-af",
                "pan=stereo| c0=c0 | c1=c1",
                "-c:v",
                "copy",
                "-c:a",
                "aac",
                "-f",
                "mp4",
                "-y",
                f"{output}.merge",
            ],
        )
        ffmpeg.wait()
        output.unlink(missing_ok=True)
        pathlib.Path(f"{output}.merge").rename(output)
        pathlib.Path(f"{output}.render").unlink()
        pathlib.Path(f"{output.with_suffix('.wav')}.render").unlink()
