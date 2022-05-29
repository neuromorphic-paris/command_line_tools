import argparse
import atexit
import os
import pathlib
import re
import subprocess
import sys

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


dirname = pathlib.Path(__file__).resolve().parent

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
    choices=["exponential", "linear", "window"],
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
    "--add-timecode", "-a", action="store_true", help="timecode overlay"
)
parser.add_argument(
    "--discard-ratio",
    "-r",
    type=float,
    default="0.01",
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
    "--av1",
    "-u",
    action="store_true",
    help="Use AV1 instead of H.264",
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
    output = (
        input.parent
        / f"{input.stem}{range_string}_{args.style}_frametime={frametime}_tau={tau}.mp4"
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

event_stream = open(args.input)
es_to_frames_arguments = [
    str(dirname / "build" / "release" / "es_to_frames"),
    f"--begin={args.begin}",
    f"--frametime={args.frametime}",
    f"--style={args.style}",
    f"--tau={args.tau}",
    f"--oncolor={args.oncolor}",
    f"--offcolor={args.offcolor}",
    f"--idlecolor={args.idlecolor}",
    f"--discard-ratio={args.discard_ratio}",
    f"--atiscolor={args.atiscolor}",
]
if args.end is not None:
    es_to_frames_arguments.append(f"--end={args.end}")
if args.add_timecode:
    es_to_frames_arguments.append("--add-timecode")
if args.white is not None:
    es_to_frames_arguments.append(f"--white={args.white}")
if args.black is not None:
    es_to_frames_arguments.append(f"--black={args.black}")
es_to_frames = subprocess.Popen(
    es_to_frames_arguments,
    stdin=event_stream,
    stdout=subprocess.PIPE,
)
assert es_to_frames.stdout is not None

ffmpeg_arguments = [
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
]
if args.av1:
    ffmpeg_arguments += [
        "librav1e",
        "-rav1e-params",
        f"speed=3:threads={os.cpu_count()}",
    ]
else:
    ffmpeg_arguments += ["libx264", "-pix_fmt", "yuv420p", "-crf", "18"]
ffmpeg_arguments += [
    "-y",
    str(output),
]
ffmpeg = subprocess.Popen(ffmpeg_arguments, stdin=subprocess.PIPE)
assert ffmpeg.stdin is not None


def cleanup():
    es_to_frames.kill()
    ffmpeg.kill()


atexit.register(cleanup)

frame_size = width * height * 3
while True:
    frame = es_to_frames.stdout.read(frame_size)
    if len(frame) != frame_size:
        break
    ffmpeg.stdin.write(frame)

ffmpeg.stdin.close()
event_stream.close()
es_to_frames.wait()
ffmpeg.wait()
