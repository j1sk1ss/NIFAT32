import argparse
import mmap
import random

from datetime import datetime

def _inject_bitflips_mmap(file_path, num_flips=10):
    with open(file_path, 'r+b') as f:
        mm = mmap.mmap(f.fileno(), 0)
        size = len(mm)
        for _ in range(num_flips):
            idx = random.randint(0, size - 1)
            bit = random.randint(0, 7)
            mm[idx] = mm[idx] ^ (1 << bit)

        mm.flush()
        mm.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Inject bitflips into disk image.")
    parser.add_argument("-s", "--size", type=int, default=10)
    parser.add_argument("-c", "--count", type=int, default=1)
    parser.add_argument("-i", "--image", type=str, default="nifat32.img")
    args = parser.parse_args()

    total_flips = 0
    for i in range(args.count):
        _inject_bitflips_mmap(args.image, args.size)
        total_flips += args.size
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{now}] Total flips: {total_flips}")
