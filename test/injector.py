import sys
import mmap
import random
import argparse
import pyfiglet
import textwrap


def random_bitflips(file_path: str, num_flips: int = 10) -> None:
    with open(file_path, "r+b") as f:
        mm: mmap.mmap = mmap.mmap(f.fileno(), 0)
        size: int = len(mm)
        for _ in range(num_flips):
            idx: int = random.randint(0, size - 1)
            bit: int = random.randint(0, 7)
            mm[idx] = mm[idx] ^ (1 << bit)

        mm.flush()
        mm.close()


def scratch_emulation(
    file_path: str,
    scratch_length: int = 1024,
    width: int = 1,
    intensity: float = 0.7
) -> None:
    """
    Emulates a physical scratch on a disk image by corrupting sequential sectors.
    
    Parameters:
        file_path (str): Path to the disk image file
        scratch_length (int): Total length of the scratch in bytes (default 1KB)
        width (int): Number of parallel corruption tracks (default 1)
        intensity (float): Damage intensity (0.0-1.0, probability of byte corruption)
    """
    with open(file_path, "r+b") as f:
        with mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_WRITE) as mm:
            size = len(mm)
            if size == 0:
                return

            start_pos: int = random.randint(0, size - scratch_length)
            start_pos: int = start_pos - (start_pos % width)
            
            for offset in range(start_pos, start_pos + scratch_length):
                if random.random() > intensity:
                    continue
                
                for track in range(width):
                    pos = offset + track
                    if pos < size:
                        flip_mask = random.randint(1, 255)
                        mm[pos] ^= flip_mask


def print_welcome(parser: argparse.ArgumentParser) -> None:
    print("Welcome! Avaliable arguments:\n")
    help_text = parser.format_help()
    options_start = help_text.find("optional arguments:")
    if options_start != -1:
        options_text = help_text[options_start:]
        print(textwrap.indent(options_text, "    "))
    else:
        print(textwrap.indent(help_text, "    "))


if __name__ == "__main__":
    ascii_banner = pyfiglet.figlet_format("Injector")
    print(ascii_banner)
    
    parser = argparse.ArgumentParser(description="Inject bitflips into disk image.")
    parser.add_argument("--strategy", type=str, default="random")
    parser.add_argument("--scratch-length", type=int, default=1024)
    parser.add_argument("--width", type=int, default=1)
    parser.add_argument("--intensity", type=float, default=.7)
    parser.add_argument("--size", type=int, default=10)
    parser.add_argument("--image", type=str, default="nifat32.img")
    args = parser.parse_args()

    if len(sys.argv) == 1:
        print_welcome(parser=parser)
        exit(1)

    if args.strategy == "random":
        random_bitflips(file_path=args.image, num_flips=args.size)
    elif args.strategy == "stretch":
        scratch_emulation(file_path=args.image, scratch_length=args.scratch_length, width=args.width, intensity=args.intensity)
