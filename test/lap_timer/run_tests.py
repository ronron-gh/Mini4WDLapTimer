"""Build and run the hardware-independent tests with a host C++ compiler."""
import argparse
import pathlib
import shutil
import subprocess
import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", help="Path/name of a GCC-compatible host C++ compiler")
    args = parser.parse_args()
    compiler = args.compiler or shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        print("No host C++ compiler found. Pass --compiler PATH.", file=sys.stderr)
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    output = root / ".pio" / "host-tests"
    output.mkdir(parents=True, exist_ok=True)
    binary = output / ("lap_timer_tests.exe" if sys.platform == "win32" else "lap_timer_tests")
    command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Werror",
               "-I" + str(root / "include"), str(root / "src" / "LapTimer.cpp"),
               str(root / "test" / "lap_timer" / "test_main.cpp"), "-o", str(binary)]
    subprocess.run(command, cwd=root, check=True)
    subprocess.run([str(binary)], cwd=root, check=True)
    return 0

if __name__ == "__main__":
    sys.exit(main())
