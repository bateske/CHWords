"""Build and run the host tests (the dictionary, the rules, game flow, the CPU).

    python tools/tests/run_tests.py [--quick]
"""
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE.parent / "chsim"))
from chsim import find_cxx  # noqa: E402

# The pure-logic sources, compiled beside the tests as they are.
SRC = ROOT / "src"
SOURCES = [HERE / "test_words.cpp", SRC / "rules" / "Words.cpp", SRC / "game" / "Game.cpp", SRC / "ai" / "Ai.cpp",
           SRC / "dict" / "FlashDict.cpp", SRC / "dict" / "DictData.cpp"]


def main():
    exe = HERE / "build" / "test_words.exe"
    exe.parent.mkdir(exist_ok=True)
    cmd = find_cxx() + ["-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter",
                        "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unknown-pragmas",
                        "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                        "-DCHTEST", *[str(s) for s in SOURCES], "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("build failed")
    if r.stderr.strip():
        sys.stderr.write(r.stderr)
    raise SystemExit(subprocess.run([str(exe), *sys.argv[1:]], cwd=ROOT).returncode)


if __name__ == "__main__":
    main()
