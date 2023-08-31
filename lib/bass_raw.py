import argparse
import subprocess
import sys
import struct


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 read_file.py <filename>")
        sys.exit(1)

    # Get the file name from the argument.
    filename = sys.argv[1]

    # Open the file.
    i = 0
    with open(filename, "rb") as fp:
        # Read the 16-bit integers from the file.
        while True:
            try:
                integer = struct.unpack("h", fp.read(2))[0]
                i += 1
                print(integer, end=",")
                if i > 0 and i % 20 == 0:
                    print("\n")
            except:
                break

    return 0


def bass(file_name):
    # sox lib/audio/bass_e.wav /tmp/1.wav fade 0.001 -0 0.001 norm gain -6
    subprocess.run(
        [
            "sox",
            file_name,
            "/tmp/1.wav",
            "fade",
            "0.001",
            "-0",
            "0.001",
            "norm",
            "gain",
            "-6",
        ]
    )

    # gcc -o /tmp/convert lib/convert.c
    subprocess.run(["gcc", "-o", "/tmp/convert", "lib/convert.c"])

    # rm -rf lib/bass_raw.h
    # touch lib/bass_raw.h
    subprocess.run(["rm", "-rf", "lib/bass_raw.h"])
    subprocess.run(["touch", "lib/bass_raw.h"])

    # sox /tmp/1.wav -c 1 --bits 16 --encoding signed-integer --endian little /tmp/0.wav
    subprocess.run(
        [
            "sox",
            "/tmp/1.wav",
            "-c",
            "1",
            "--bits",
            "16",
            "--encoding",
            "signed-integer",
            "--endian",
            "little",
            "/tmp/0.wav",
        ]
    )

    # sox /tmp/0.wav -t raw /tmp/0.raw
    subprocess.run(["sox", "/tmp/0.wav", "-t", "raw", "/tmp/0.raw"])

    # /tmp/convert /tmp/0.raw 0 >> lib/bass_raw.h
    subprocess.run(["/tmp/convert", "/tmp/0.raw", "0", ">>", "lib/bass_raw.h"])

    # sox /tmp/0.wav /tmp/1.wav speed 2.0
    subprocess.run(["sox", "/tmp/0.wav", "/tmp/1.wav", "speed", "2.0"])

    # sox /tmp/1.wav -t raw /tmp/1.raw
    subprocess.run(["sox", "/tmp/1.wav", "-t", "raw", "/tmp/1.raw"])

    # /tmp/convert /tmp/1.raw 1 >> lib/bass_raw.h
    subprocess.run(["/tmp/convert", "/tmp/1.raw", "1", ">>", "lib/bass_raw.h"])


if __name__ == "__main__":
    main()
    # parser = argparse.ArgumentParser()
    # parser.add_argument("file_name", help="The path to the audio file")
    # args = parser.parse_args()

    # bass(args.file_name)
