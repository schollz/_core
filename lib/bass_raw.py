import argparse
import subprocess
import sys
import struct
from tqdm import tqdm


def raw16bit(filename):
    # Open the file.
    i = 0
    s = ""
    with open(filename, "rb") as fp:
        # Read the 16-bit integers from the file.
        while True:
            try:
                integer = struct.unpack("h", fp.read(2))[0]
                i += 1
                s += "{},".format(integer)
                if i > 0 and i % 20 == 0:
                    s += "\n\t"
            except:
                break
    return s, i


def bass(file_in, file_out):
    # 16 steps:
    ratios = [
        # 0.79370052598483,
        # 0.8408964152543,
        # 0.89089871814075,
        # 0.94387431268191,
        # 1.0,
        # 1.0594630943591,
        # 1.1224620483089,
        # 1.1892071150019,
        # 1.2599210498937,
        # 1.3348398541685,
        # 1.4142135623711,
        # 1.4983070768743,
        # 1.5874010519653,
        # 1.6817928305039,
        1.7817974362766,
        # 1.8877486253586,
    ]
    subprocess.run(
        [
            "sox",
            file_in,
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

    with open(file_out, "w") as f:
        for i, ratio in tqdm(enumerate(ratios)):
            subprocess.run(
                [
                    "sox",
                    "/tmp/1.wav",
                    "/tmp/2.wav",
                    "speed",
                    "{}".format(ratio),
                ]
            )

            # sox /tmp/1.wav -c 1 --bits 16 --encoding signed-integer --endian little /tmp/0.wav
            subprocess.run(
                [
                    "sox",
                    "/tmp/2.wav",
                    "-c",
                    "1",
                    "--bits",
                    "16",
                    "--encoding",
                    "signed-integer",
                    "--endian",
                    "little",
                    "/tmp/3.wav",
                ]
            )

            subprocess.run(["sox", "/tmp/3.wav", "-t", "raw", "/tmp/4.raw"])

            f.write(f"const int16_t __in_flash() bass_raw_{i}[] = {{\n")
            byte_string, byte_len = raw16bit("/tmp/4.raw")
            f.write(byte_string)
            f.write("\n};\n\n")

            f.write(f"static uint32_t BASS_RAW_{i}_LEN={byte_len};\n\n")

            # # sox /tmp/0.wav -t raw /tmp/0.raw

            # # /tmp/convert /tmp/0.raw 0 >> lib/bass_raw.h
            # subprocess.run(["/tmp/convert", "/tmp/0.raw", "0", ">>", "lib/bass_raw.h"])

            # # sox /tmp/0.wav /tmp/1.wav speed 2.0
            # subprocess.run(["sox", "/tmp/0.wav", "/tmp/1.wav", "speed", "2.0"])

            # # sox /tmp/1.wav -t raw /tmp/1.raw
            # subprocess.run(["sox", "/tmp/1.wav", "-t", "raw", "/tmp/1.raw"])

            # # /tmp/convert /tmp/1.raw 1 >> lib/bass_raw.h
            # subprocess.run(["/tmp/convert", "/tmp/1.raw", "1", ">>", "lib/bass_raw.h"])
        f.write("uint32_t bass_len(uint8_t i) {\n")
        for i in range(len(ratios)):
            if i == 0:
                f.write("if ")
            else:
                f.write("else if ")
            f.write(f"(i == {i}) {{\n")
            f.write(f"\treturn BASS_RAW_{i}_LEN;\n")
            f.write("}")
        f.write("\nreturn 0;\n}\n\n")

        f.write("int16_t bass_sample(uint8_t i, uint32_t phase) {\n")
        for i in range(len(ratios)):
            if i == 0:
                f.write("if ")
            else:
                f.write("else if ")
            f.write(f"(i == {i}) {{\n")
            f.write(f"\treturn bass_raw_{i}[phase];\n")
            f.write("}")
        f.write("\nreturn 0;\n}\n\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("audio", help="audio filepath")
    parser.add_argument("out", help="output file")
    args = parser.parse_args()
    bass(args.audio, args.out)
