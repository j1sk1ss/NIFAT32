import os
import sys
import shutil
import textwrap
import pyfiglet
import argparse
import subprocess

from loguru import logger


def build_image(
    formatter: str, 
    clean: bool = True,
    spc: int = 8, v_size: int = 64, bs_count: int = 5, fc: int = 5, jc: int = 0, 
    output: str = "nifat32.img"
) -> None:
    """
    Create nifat32 image by building formatter and creating image.
    Note: After building formatter and creating an image, will move it to root.
    """
    
    original_dir = os.getcwd()
    formatter_dir = os.path.abspath(formatter)
    formatter_bin = os.path.join(formatter_dir, "formatter")
    output_image = os.path.join(original_dir, output)

    try:
        logger.info(f"Building formatter in {formatter_dir}")
        os.chdir(formatter_dir)
        subprocess.run(["make"], check=True)

        build_seq: list = [
            formatter_bin, 
            "-o", output, 
            "-s", "nifat32", 
            "--volume-size", str(v_size), 
            "--spc", str(spc), 
            "--fc", str(fc), 
            "--bsbc", str(bs_count),
            "--jc", str(jc)
        ]

        logger.info(f"Running formatter to create nifat32.img args={build_seq}")
        subprocess.run(build_seq, check=True)

        if clean:
            logger.info("Removing formatter binary")
            os.remove(formatter_bin)

        os.chdir(original_dir)
        logger.info("Moving nifat32.img to root directory")
        shutil.move(os.path.join(formatter_dir, output), output_image)

        logger.info("Switching to test directory")
        os.chdir("test")
    except subprocess.CalledProcessError as e:
        logger.error(f"Command failed with exit code {e.returncode}: {e.cmd}")
        exit(1)
    except FileNotFoundError as e:
        logger.error(f"File not found: {e.filename}")
        exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        exit(1)


def build_nifat32(nifat32_root: str) -> None:
    pass


def run_index_test() -> None:
    pass


def run_benchmark() -> None:
    pass


def run_default_tests(create_files: bool = True) -> None:
    pass


def run_birflips_test(scenarion: str) -> None:
    from injector import random_bitflips, scratch_emulation
    pass


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
    ascii_banner = pyfiglet.figlet_format("NIFAT32 Tester")
    print(ascii_banner)
    
    parser = argparse.ArgumentParser(description="NIFAT32 tests")
    
    # === Main setup ===
    parser.add_argument("--debug", nargs="+", help="Debug flags (e.g. debug, log, info, error, special)")
    parser.add_argument("--test-type", type=str, default="default", help="Test type: benchmark, index, default, bitflip.")
    parser.add_argument("--new-image", action="store_true", help="Build a new nifat32 image for test.")
    parser.add_argument("--clean", action="store_true", help="Clean directory from build files and binary files.")
    parser.add_argument("--test-size", type=int, default=1000, help="Test size.")
    
    # === Formatter setup ===
    parser.add_argument("--formatter", type=str, help="Path to formatter tool.")
    parser.add_argument("--tests-folder", type=str, help="Path to tests folder. (Test name example: test_*.c).")
    parser.add_argument("--root-folder", type=str, help="Path to NIFAT32 root folder (With nifat32.h).")
    parser.add_argument("--image-size", type=int, default=64, help="Image size on MB")
    parser.add_argument("--spc", type=int, default=8, help="Sectors per cluster")
    parser.add_argument("--bs-count", type=int, default=5, help="Bootsector count")
    parser.add_argument("--j-count", type=int, default=0, help="Journals count")
    parser.add_argument("--fat-count", type=int, default=5, help="FAT count")
    
    # === Bitflip setup ===
    parser.add_argument("--bitflips-strategy", type=str, default="random", help="Bitflip strategy: random or scratch.")
    parser.add_argument("--scratch-length", type=int, default=1024)
    parser.add_argument("--scratch-width", type=int, default=1)
    parser.add_argument("--scratch-intensity", type=float, default=.7)
    parser.add_argument("--bitflips-count", type=int, default=10)
    
    # === Injection setup ===
    parser.add_argument("--injector-scenario", type=str, help="Path to injector scenario.")
    args = parser.parse_args()
    
    if len(sys.argv) == 1:
        print_welcome(parser=parser)
        exit(1)
        
    # === Log setup options ===
    logger.info(f"Test starting. Type={args.test_type}")
    logger.info(f"Test size: {args.test_size}")
    
    if args.debug:
        logger.info(f"Debug flags: {args.debug}")
        
    if args.new_image:
        logger.info("A new NIFAT32 image will be created for the test.")
        logger.info(f"Image size: {args.image_size} MB")
        logger.info(f"Bootsector count: {args.bs_count}")
        logger.info(f"Journal count: {args.j_count}")
        logger.info(f"FAT count: {args.fat_count}")
        if args.formatter:
            logger.info(f"Formatter tool path: {args.formatter}")
            
        build_image(
            formatter=args.formatter, 
            clean=args.clean,
            spc=args.spc, v_size=args.image_size, bs_count=args.bs_count, fc=args.fat_count, jc=args.j_count
        )
     
    if args.test_type not in ["benchmark", "index", "default", "bitflip"]:
        logger.error(f"Unknown test type: {args.test_type}")
        
    if args.clean:
        logger.info("Temporary build files and binary files will be cleaned after the test.")
        
    if args.tests_folder:
        logger.info(f"Tests folder path: {args.tests_folder}")
    if args.root_folder:
        logger.info(f"NIFAT32 root folder path: {args.root_folder}")
        
    if args.test_type == "bitflip":
        logger.info("Bitflip testing mode selected.")
        logger.info(f"Bitflip strategy: {args.bitflips_strategy}")
        logger.info(f"Bitflips count: {args.bitflips_count}")
        if args.injector_scenario:
            logger.info(f"Injector scenario path: {args.injector_scenario}")
            
        if args.bitflips_strategy == "scratch":
            logger.info(f"Scratch length: {args.scratch_length}")
            logger.info(f"Scratch width: {args.scratch_width}")
            logger.info(f"Scratch intensity: {args.scratch_intensity}")
