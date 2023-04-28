import argparse
import atexit
import pathlib
import re
import subprocess
import sys
import tempfile
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
parser.add_argument("input", help="input .es file or directory")
parser.add_argument(
    "--output",
    "-o",
    help="output .mp4 file, calculated from the input file name if not provided",
)
parser.add_argument(
    "--begin",
    "-b",
    type=timecode,
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
    "--scale",
    "-c",
    type=int,
    default=1,
    help="scale up the output by this integer factor",
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
parser.add_argument("--idlecolor", "-l", default="#191919", help="background color")
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
    "--no-timecode", "-a", action="store_true", help="do not add a timecode overlay"
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
    "--no-merge",
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
    "--sonify-activity-tau",
    type=timecode,
    default=10000,
    help="row decay parameter in µs",
)
parser.add_argument(
    "--no-mp4",
    action="store_true",
    help="do not render mp4 videos",
)
parser.add_argument(
    "--rainbow",
    "-p",
    action="store_true",
    help="render rainbow plots",
)
parser.add_argument(
    "--rainbow-alpha",
    type=float,
    default=0.1,
    help="transparency level for each event in the rainbow plot",
)
parser.add_argument(
    "--rainbow-idlecolor",
    default="#191919",
    help="background color for the rainbow plot",
)
args = parser.parse_args()

no_merge = args.no_merge
if args.no_mp4:
    no_merge = True
if args.no_mp4 and not args.rainbow and not args.sonify:
    sys.stderr.write("--no-mp4 requires --rainbow or --sonify\n")
    sys.exit(1)

active: dict[str, typing.Optional[subprocess.Popen]] = {
    "es_to_frames": None,
    "ffmpeg": None,
    "synth": None,
}


def cleanup():
    for name in list(active.keys()):
        popen = active[name]
        if popen is not None:
            popen.kill()
            active[name] = None


atexit.register(cleanup)


def render(
    input_file: pathlib.Path,
    output_file_or_directory: typing.Optional[pathlib.Path],
):
    print(f"\033[1m{input_file}\033[0m")
    output_parent: typing.Optional[pathlib.Path]
    if output_file_or_directory is None:
        output_parent = input_file.parent
    elif output_file_or_directory.is_dir():
        output_parent = output_file_or_directory
    else:
        output_parent = None
    range_string = ""
    if args.begin is not None or args.end is not None:
        range_string += f"_begin={0 if args.begin is None else args.begin}"
    if args.end is not None:
        range_string += f"_end={args.end}"
    if (args.frametime // 1e6) * 1e6 == args.frametime:
        frametime_string = f"{args.frametime // 1000000}s"
    elif (args.frametime // 1e3) * 1e3 == args.frametime:
        frametime_string = f"{args.frametime // 1000}ms"
    else:
        frametime_string = f"{args.frametime}us"
    if (args.tau // 1e6) * 1e6 == args.tau:
        tau_string = f"{args.tau // 1000000}s"
    elif (args.tau // 1e3) * 1e3 == args.tau:
        tau_string = f"{args.tau // 1000}ms"
    else:
        tau_string = f"{args.tau}us"
    if output_parent is None:
        assert output_file_or_directory is not None
        rainbow_output_file = output_file_or_directory.with_suffix(".png")
    else:
        rainbow_output_file = (
            output_parent / f"{input_file.stem}_alpha={args.rainbow_alpha}.png"
        )
    if args.rainbow:
        print(f"🌈 {rainbow_output_file}")
        if args.begin is None and args.end is None:
            subprocess.run(
                (
                    str(dirname / "build" / "release" / "rainbow"),
                    str(input_file),
                    str(rainbow_output_file),
                    f"--alpha={args.rainbow_alpha}",
                    f"--idlecolor={args.rainbow_idlecolor}",
                ),
                check=True,
            )
        else:
            with tempfile.TemporaryDirectory(
                prefix="command-line-tools-render"
            ) as temporary_directory_string:
                temporary_directory = pathlib.Path(temporary_directory_string)
                subprocess.run(
                    (
                        str(dirname / "build" / "release" / "cut"),
                        str(input_file),
                        str(temporary_directory / input_file.name),
                        str(0 if args.begin is None else args.begin),
                        str((2**64 - 1) if args.end is None else args.end),
                        "--timestamp",
                        "zero",
                    ),
                    check=True,
                )
                subprocess.run(
                    (
                        str(dirname / "build" / "release" / "rainbow"),
                        str(temporary_directory / input_file.name),
                        str(rainbow_output_file),
                        f"--alpha={args.rainbow_alpha}",
                        f"--idlecolor={args.rainbow_idlecolor}",
                    ),
                    check=True,
                )
    sonify_suffix = "_sound" if args.sonify else ""
    if output_parent is None:
        assert output_file_or_directory is not None
        mp4_output = output_file_or_directory.with_suffix(".mp4")
    else:
        mp4_output = (
            output_parent
            / f"{input_file.stem}{range_string}_{args.style}_frametime={frametime_string}_tau={tau_string}{sonify_suffix}.mp4"
        )
    if not args.no_mp4:
        if args.sonify:
            print(f"🎬 {mp4_output} (video only)")
        else:
            print(f"🎬 {mp4_output}")
        width, height = (
            int(value)
            for value in subprocess.run(
                [str(dirname / "build" / "release" / "size"), str(input_file)],
                check=True,
                capture_output=True,
            ).stdout.split(b"x")
        )
        width *= args.scale
        height *= args.scale
        es_to_frames_arguments = [
            str(dirname / "build" / "release" / "es_to_frames"),
            f"--input={str(input_file)}",
            f"--begin={0 if args.begin is None else args.begin}",
            f"--frametime={args.frametime}",
            f"--scale={args.scale}",
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
        if not args.no_timecode:
            es_to_frames_arguments.append("--add-timecode")
        if args.white is not None:
            es_to_frames_arguments.append(f"--white={args.white}")
        if args.black is not None:
            es_to_frames_arguments.append(f"--black={args.black}")
        active["es_to_frames"] = subprocess.Popen(
            es_to_frames_arguments,
            stdout=subprocess.PIPE,
        )
        assert active["es_to_frames"].stdout is not None
        active["ffmpeg"] = subprocess.Popen(
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
                f"{mp4_output}.render",
            ],
            stdin=subprocess.PIPE,
        )
        assert active["ffmpeg"].stdin is not None
        frame_size = width * height * 3
        while True:
            frame = active["es_to_frames"].stdout.read(frame_size)
            if len(frame) != frame_size:
                break
            active["ffmpeg"].stdin.write(frame)

        active["ffmpeg"].stdin.close()
        active["es_to_frames"].wait()
        active["es_to_frames"] = None
        active["ffmpeg"].wait()
        active["ffmpeg"] = None
        if not args.sonify or no_merge:
            mp4_output.unlink(missing_ok=True)
            pathlib.Path(f"{mp4_output}.render").replace(mp4_output)

    if args.sonify:
        if output_parent is None:
            assert output_file_or_directory is not None
            wav_output = output_file_or_directory.with_suffix(".wav")
        else:
            if args.no_mp4:
                wav_output = (
                    output_parent
                    / f"{input_file.stem}{range_string}_frametime={frametime_string}.wav"
                )
            else:
                wav_output = mp4_output.with_suffix(".wav")
        print(f"🔊 {wav_output}")
        synth_arguments = [
            str(dirname / "build" / "release" / "synth"),
            str(input_file),
            f"{wav_output}.render",
            "--output-mode=1",
            f"--begin={0 if args.begin is None else args.begin}",
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
        active["synth"] = subprocess.Popen(
            synth_arguments,
            stdout=subprocess.PIPE,
        )
        assert active["synth"].stdout is not None
        while True:
            line = active["synth"].stdout.readline()
            if len(line) == 0:
                sys.stdout.write(f"\n")
                break
            sys.stdout.write(f"\r{line[:-1].decode()}")
            sys.stdout.flush()
        active["synth"].wait()
        active["synth"] = None
        if no_merge:
            wav_output.unlink(missing_ok=True)
            pathlib.Path(f"{wav_output}.render").replace(wav_output)
        else:
            print(f"🎬+🔊 {mp4_output}")
            active["ffmpeg"] = subprocess.Popen(
                [
                    args.ffmpeg,
                    "-hide_banner",
                    "-loglevel",
                    "warning",
                    "-stats",
                    "-i",
                    f"{mp4_output}.render",
                    "-guess_layout_max",
                    "0",
                    "-i",
                    f"{wav_output}.render",
                    "-af",
                    "pan=stereo| c0=c0 | c1=c1",
                    "-c:v",
                    "copy",
                    "-c:a",
                    "aac",
                    "-f",
                    "mp4",
                    "-y",
                    f"{mp4_output}.merge",
                ],
            )
            active["ffmpeg"].wait()
            active["ffmpeg"] = None
            mp4_output.unlink(missing_ok=True)
            pathlib.Path(f"{mp4_output}.merge").replace(mp4_output)
            pathlib.Path(f"{mp4_output}.render").unlink()
            pathlib.Path(f"{wav_output}.render").unlink()


def render_directory(
    input_directory: pathlib.Path,
    output_directory: pathlib.Path,
):
    children = []
    for child in sorted(input_directory.iterdir()):
        if child.is_dir():
            children.append(child)
        elif child.is_file() and child.suffix == ".es":
            output_directory.mkdir(parents=True, exist_ok=True)
            render(child, output_directory)
    for child in children:
        render_directory(child, output_directory / child.name)


input = pathlib.Path(args.input).resolve()
if input.is_dir():
    if args.output is None:
        render_directory(input, input)
    else:
        output = pathlib.Path(args.output).resolve()
        if output.exists() and not output.is_dir():
            sys.stderr.write(
                "--output must be a directory (out of tree generation) or unspecified (in tree generation) if the input is a directory\n"
            )
            sys.exit(1)
        render_directory(input, output)
else:
    if not pathlib.Path(args.input).exists():
        sys.stderr.write(f"{input} does not exist\n")
        sys.exit(1)
    if not pathlib.Path(args.input).is_file():
        sys.stderr.write(f"{input} is neither a file nor a directory\n")
        sys.exit(1)
    if args.output is None:
        render(input, None)
    else:
        output = pathlib.Path(args.output).resolve()
        if input == output:
            sys.stderr.write("input and output must be different files\n")
            sys.exit(1)
        render(input, output)
