import os
import sys
import shutil
import textwrap
import pyfiglet
import argparse
import subprocess

from pathlib import Path
from loguru import logger


def build_image(
    formatter: str, 
    clean: bool = True,
    spc: int = 8, v_size: int = 64, bs_count: int = 5, fc: int = 5, jc: int = 0, ec: int = 0,
    output: str = "nifat32.img"
) -> str:
    """
    Create nifat32 image by building formatter and creating image.
    Note: After building formatter and creating an image, will move it to root.
    Args:
        formatter (str): Path to formatter directory
        clean (bool, optional): Clean all binary files after building. Defaults to True.
        spc (int, optional): Sectors per cluster. Defaults to 8.
        v_size (int, optional): Volume size. Defaults to 64.
        bs_count (int, optional): BootSector count. Defaults to 5.
        fc (int, optional): FAT count. Defaults to 5.
        jc (int, optional): Journals (copies) count. Defaults to 0.
        output (str, optional): Output location for image. Defaults to "nifat32.img".
    """
    
    original_dir  = os.getcwd()
    formatter_dir = os.path.abspath(formatter)
    formatter_bin = os.path.join(formatter_dir, "formatter")
    output_image  = os.path.join(original_dir, output)

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
            "--jc", str(jc),
            "--ec", str(ec)
        ]

        logger.info(f"Running formatter to create nifat32.img args={build_seq}")
        subprocess.run(build_seq, check=True)

        if clean:
            logger.info("Removing formatter binary")
            os.remove(formatter_bin)

        os.chdir(original_dir)
        logger.info("Moving nifat32.img to root directory")
        shutil.move(os.path.join(formatter_dir, output), output_image)
    except subprocess.CalledProcessError as e:
        logger.error(f"Command failed with exit code {e.returncode}: {e.cmd}")
        exit(1)
    except FileNotFoundError as e:
        logger.error(f"File not found: {e.filename}")
        exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        exit(1)
        
    return output


def build_test(
    nifat32_path: list[str], base_path: str, test_path: str,  debug: list | None, creation: bool = True, compiler: str = "gcc-14", include_path: str = "include/"
) -> Path:
    """
    Build separated test
    Args:
        nifat32_path (list[str]): nifat32 related info. Path to nifat32_test.h, nifat32.c, src/*, std/*
        test_path (str): Path to test_*.c
        debug (list | None): Debug flags

    Raises:
        FileNotFoundError: There is no test_*.c file
        e: Something goes wrong during compilation
    """
    
    logger.info(f"Building nifat32_path={nifat32_path}, base_path={base_path}, test_path={test_path}, debug={debug}, creation={creation}, compiler={compiler}, include_path={include_path}")
    test_path: Path = Path(test_path)
    if not test_path.exists():
        raise FileNotFoundError(f"Test file not found: {test_path}")

    bin_name = test_path.stem
    output_path: Path = Path(base_path) / bin_name

    build_flags: list[str] = [ f"-I{include_path}" ]
    if not creation:
        build_flags.append("-DNO_CREATION")
    
    if debug:
        for i in debug:
            build_flags.append({
               "error":  "-DERROR_LOGS", "warn": "-DWARNING_LOGS", "debug": "-DDEBUG_LOGS",
               "info": "-DINFO_LOGS", "log": "-DLOGGING_LOGS"
            }[i])
            
        build_flags.append("-g")
        
    compile_cmd: list[str] = [
        compiler, *build_flags,
        str(test_path),
        *nifat32_path, # nifat32_test.h nifat32.c src/* std/*
        "-o", str(output_path)
    ]

    logger.info(f"Compiling {bin_name}, cmd={compile_cmd}...")

    try:
        subprocess.run(" ".join(compile_cmd), shell=True, check=True)
        logger.info(f"Compilation succeeded: {output_path}")
    except subprocess.CalledProcessError as e:
        logger.error(f"Compilation failed: {bin_name}, e={e}")
        raise e
    
    return output_path


def print_welcome(parser: argparse.ArgumentParser) -> None:
    print("Welcome! Avaliable arguments:\n")
    help_text = parser.format_help()
    options_start = help_text.find("optional arguments:")
    if options_start != -1:
        options_text = help_text[options_start:]
        print(textwrap.indent(options_text, "    "))
    else:
        print(textwrap.indent(help_text, "    "))


def run_tests(test_bins: list[Path], test_size: int) -> bool:
    for bin_path in test_bins:
        logger.info(f"Running test: {bin_path.name} with size={test_size}")
        try:
            result = subprocess.run([str(bin_path), str(test_size)], check=True)
            if result.returncode == 1:
                logger.error(f"Test {bin_path.name} unexpectedly exited with code 0 (expected failure).")
            else:
                logger.info(f"Test {bin_path.name} exited with code {result.returncode} (as expected).")
        except subprocess.CalledProcessError as e:
            logger.error(f"Test {bin_path.name} failed with error: {e}")
            return False
            
    return True


if __name__ == "__main__":
    ascii_banner = pyfiglet.figlet_format("NIFAT32 Tester")
    print(ascii_banner)
    
    parser = argparse.ArgumentParser(description="NIFAT32 tests")
    
    # === Main setup ===
    parser.add_argument("--debug", nargs="+", help="Debug flags (e.g. debug, log, info, error, special)")
    parser.add_argument("--test-type", type=str, default="default", help="Test type: default, bitflip.")
    parser.add_argument("--new-image", action="store_true", help="Build a new nifat32 image for test.")
    parser.add_argument("--clean", action="store_true", help="Clean directory from build files and binary files.")
    parser.add_argument("--test-size", type=int, default=1000, help="Test size.")
    parser.add_argument("--compiler", type=str, default="gcc-14", help="Compiler for compilation.")
    
    # === Formatter setup ===
    parser.add_argument("--formatter", type=str, help="Path to formatter tool.")
    parser.add_argument("--tests-folder", type=str, help="Path to tests folder. (Test name example: test_*.c).")
    parser.add_argument("--root-folder", type=str, help="Path to NIFAT32 root folder (With nifat32.h).")
    parser.add_argument("--image-size", type=int, default=64, help="Image size on MB")
    parser.add_argument("--spc", type=int, default=8, help="Sectors per cluster")
    parser.add_argument("--bs-count", type=int, default=5, help="Bootsector count")
    parser.add_argument("--j-count", type=int, default=0, help="Journals count")
    parser.add_argument("--e-count", type=int, default=0, help="Error-code count")
    parser.add_argument("--fat-count", type=int, default=5, help="FAT count")
    
    # === Bitflip setup ===
    parser.add_argument("--bitflips-strategy", type=str, default="random", help="Bitflip strategy: random or scratch.")
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
    if args.test_type not in ["benchmark", "index", "default", "bitflip"]:
        logger.error(f"Unknown test type: {args.test_type}")
        exit(1)
        
    logger.info(f"Test size: {args.test_size}")
    if args.clean:
        logger.info("Temporary build files and binary files will be cleaned after the test.")
        
    if args.debug:
        logger.info(f"Debug flags: {args.debug}")
                
    # === Build nifat32 image ===
    image_path: str = "nifat32.img"
    if args.new_image:
        logger.info("A new NIFAT32 image will be created for the test.")
        logger.info(f"Image size:       {args.image_size} MB")
        logger.info(f"Bootsector count: {args.bs_count}")
        logger.info(f"Journal count:    {args.j_count}")
        logger.info(f"Error-code count: {args.e_count}")
        logger.info(f"FAT count:        {args.fat_count}")
        if args.formatter:
            logger.info(f"Formatter tool path: {args.formatter}")
            
        image_path = build_image(
            formatter=args.formatter, 
            clean=args.clean,
            spc=args.spc, v_size=args.image_size, bs_count=args.bs_count, fc=args.fat_count, jc=args.j_count, ec=args.e_count
        )
    
    # === Build tests ===
    if args.tests_folder and args.root_folder:
        logger.info(f"Tests folder path:        {args.tests_folder}")
        logger.info(f"NIFAT32 root folder path: {args.root_folder}")
        
        tests_path: Path = Path(args.tests_folder)
        test_files: list = sorted(tests_path.glob("test_*.c"))
        logger.info(f"tests_path={tests_path}, test_files={test_files}")
        
        test_bins: list[Path] = []
        for test in test_files:
            test_bins.append(build_test(
                nifat32_path=[
                    str(tests_path / "nifat32_test.h"),
                    str(Path(args.root_folder) / "nifat32.c"),
                    str(Path(args.root_folder) / "src/*"),
                    str(Path(args.root_folder) / "std/*")
                ], base_path=str(tests_path), test_path=test, debug=args.debug, compiler=args.compiler, include_path=str(Path(args.root_folder) / "include")
            ))
    
    # === Test running ===
    if args.test_type == "bitflip":
        logger.info("Bitflip testing mode selected.")
        logger.info(f"Bitflip strategy: {args.bitflips_strategy}")
        logger.info(f"Bitflips count:   {args.bitflips_count}")
        
        scenario: list[tuple[str, int]] = []

        if args.injector_scenario:
            logger.info(f"Injector scenario path: {args.injector_scenario}")
            try:
                with open(args.injector_scenario, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if not line or line.startswith("#"):
                            continue
                        
                        parts = line.split()
                        if len(parts) != 2 or parts[0] not in {"T", "I", "S"}:
                            logger.warning(f"Ignoring invalid scenario line: {line}")
                            continue
                        
                        scenario.append((parts[0], int(parts[1])))
            except Exception as e:
                logger.error(f"Failed to load scenario: {e}")
                scenario = [("T", 1000), ("I", 100000), ("T", 1000), ("I", 100000), ("T", 1000), ("I", 100000), ("T", 1000)]
                logger.warning(f"Using default fallback scenario={scenario}")
        else:
            scenario = [("T", 1000), ("I", 100000), ("T", 1000), ("I", 100000), ("T", 1000), ("I", 100000), ("T", 1000)]
            logger.warning(f"Injector scenario not provided. Using default fallback scenario={scenario}.")

        if args.bitflips_strategy == "scratch":
            logger.info(f"Scratch width: {args.scratch_width}")
            logger.info(f"Scratch intensity: {args.scratch_intensity}")
            
        from injector import random_bitflips, scratch_emulation
        
        bitflips_count: int = 0
        no_creation: bool = False
        for act, size in scenario:
            logger.info(f"Running scenario act={act}, total_bitlips={bitflips_count}")
            
            # Testing
            if act == "T":
                logger.info(f"Forward tesing, size={size}...")
                if not run_tests(test_bins=test_bins, test_size=size):
                    logger.error(f"Test error! bitflips={bitflips_count}")
                    exit(1)
                
                if not no_creation:
                    logger.info(f"Recompiling tests for bitflip testing on existed entries...")
                    no_creation = True
                    test_bins.clear()
                    for test in test_files:
                        test_bins.append(build_test(
                            nifat32_path=[
                                str(tests_path / "nifat32_test.h"),
                                str(Path(args.root_folder) / "nifat32.c"),
                                str(Path(args.root_folder) / "src/*"),
                                str(Path(args.root_folder) / "std/*")
                            ], base_path=str(tests_path), test_path=test, debug=args.debug, creation=False
                        ))
            # Injection
            elif act == "I":
                bitflips_count += size
                logger.info(f"Bitflip injection, size={size}...")
                random_bitflips(file_path=image_path, num_flips=size)
            # Scratch
            elif act == "S":
                bitflips_count += size * args.scratch_width * args.scratch_intensity
                logger.info(f"Scratch injection, size={size}...")
                scratch_emulation(file_path=image_path, scratch_length=size, width=args.scratch_width, intensity=args.scratch_intensity)
            else:
                logger.warning(f"Incorrect scenario action! act={act}")
    else:
        logger.info("Running compiled tests sequentially...")
        run_tests(test_bins=test_bins, test_size=args.test_size)

    # === Cleaning ===
    if args.clean:
        logger.info("Cleaning up test binaries...")
        for bin_path in test_bins:
            try:
                bin_path.unlink()
                logger.info(f"Removed binary: {bin_path}")
            except Exception as e:
                logger.warning(f"Failed to remove {bin_path}: {e}")
