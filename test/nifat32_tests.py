import os
import sys
import copy
import time
import shutil
import textwrap
import pyfiglet
import argparse
import subprocess

from pathlib import Path
from loguru import logger

COLLECTABLE_ARGS: dict[str, str] = {
    "fat-count": "fat_count",
    "bs-count": "bs_count",
    "b-bs-count": "b_bs_count",
    "j-count": "j_count",
    "e-count": "e_count",
    "spc": "spc",
    "image-size": "image_size",
    "test-size": "test_size",
}


def _build_nifat32_image_base(
    formatter: str,
    clean: bool = True,
    spc: int = 8,
    v_size: int = 64,
    bs_count: int = 5,
    b_bs_count: int = 0,
    fc: int = 5,
    jc: int = 0,
    ec: int = 0,
    output: str = "nifat32.img",
) -> str:
    original_dir = os.getcwd()
    formatter_dir = os.path.abspath(formatter)
    formatter_bin = os.path.join(formatter_dir, "formatter")
    output_image = os.path.join(original_dir, output)

    try:
        logger.info(f"Building formatter in {formatter_dir}")
        os.chdir(formatter_dir)
        subprocess.run(["make"], check=True)

        build_seq: list[str] = [
            formatter_bin,
            "-o", output,
            "-s", "nifat32",
            "--volume-size", str(v_size),
            "--spc", str(spc),
            "--fc", str(fc),
            "--bsbc", str(bs_count),
            "--b-bsbc", str(b_bs_count),
            "--jc", str(jc),
            "--ec", str(ec),
        ]

        logger.info(f"Running formatter to create {output} args={build_seq}")
        subprocess.run(build_seq, check=True)

        if clean and os.path.exists(formatter_bin):
            logger.info("Removing formatter binary")
            os.remove(formatter_bin)

        os.chdir(original_dir)
        logger.info(f"Moving {output} to root directory")
        shutil.move(os.path.join(formatter_dir, output), output_image)
    except subprocess.CalledProcessError as e:
        logger.error(f"Command failed with exit code {e.returncode}: {e.cmd}")
        raise SystemExit(1)
    except FileNotFoundError as e:
        logger.error(f"File not found: {e.filename}")
        raise SystemExit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        raise SystemExit(1)

    return output


def _build_nifat32_test(
    nifat32_path: list[str],
    base_path: str,
    test_path: str,
    debug: list | None,
    creation: bool = True,
    compiler: str = "gcc-14",
    include_path: str = "include/",
    flags: list[str] | None = None,
    output_suffix: str = "",
) -> Path:
    logger.info(
        f"Building nifat32_path={nifat32_path}, base_path={base_path}, test_path={test_path}, "
        f"debug={debug}, creation={creation}, compiler={compiler}, include_path={include_path}, output_suffix={output_suffix}"
    )
    test_path = Path(test_path)
    if not test_path.exists():
        raise FileNotFoundError(f"Test file not found: {test_path}")

    bin_name = f"{test_path.stem}{output_suffix}"
    output_path = Path(base_path) / bin_name

    build_flags: list[str] = [f"-I{include_path}"]
    if not creation:
        build_flags.append("-DNO_CREATION")

    if flags:
        build_flags.extend(flags)

    if debug:
        for i in debug:
            build_flags.append({
                "error": "-DERROR_LOGS",
                "warn": "-DWARNING_LOGS",
                "debug": "-DDEBUG_LOGS",
                "info": "-DINFO_LOGS",
                "log": "-DLOGGING_LOGS",
            }[i])
        build_flags.append("-g")

    compile_cmd: list[str] = [
        compiler,
        *build_flags,
        str(test_path),
        *nifat32_path,
        "-o", str(output_path),
    ]

    logger.info(f"Compiling {bin_name}, cmd={compile_cmd}...")

    try:
        subprocess.run(" ".join(compile_cmd), shell=True, check=True)
        logger.info(f"Compilation succeeded: {output_path}")
    except subprocess.CalledProcessError as e:
        logger.error(f"Compilation failed: {bin_name}, e={e}")
        raise

    return output_path


def _collect_compile_flags(args: argparse.Namespace) -> list[str]:
    return [
        f"-DV_SIZE={args.image_size}",
        f"-DBS_COUNT={args.bs_count}",
        f"-DB_BS_COUNT={args.b_bs_count}",
        f"-DJ_COUNT={args.j_count}",
        f"-DE_COUNT={args.e_count}",
        f"-DFC_COUNT={args.fat_count}",
        f"-DSPC={args.spc}",
    ]


def _get_selected_test_files(args: argparse.Namespace) -> list[Path]:
    tests_path = Path(args.tests_folder)
    all_tests = sorted(tests_path.glob("test_*.c"))
    selected_tests: list[Path] = []
    for test in all_tests:
        if args.test_specific and args.test_specific not in str(test):
            continue
        selected_tests.append(test)

    logger.info(f"tests_path={tests_path}, selected_tests={selected_tests}")
    return selected_tests


def _compile_test_files(
    args: argparse.Namespace,
    test_files: list[Path],
    creation: bool = True,
    output_suffix: str = "",
) -> list[Path]:
    tests_path = Path(args.tests_folder)
    test_bins: list[Path] = []

    for test in test_files:
        test_bins.append(_build_nifat32_test(
            nifat32_path=[
                str(tests_path / "nifat32_test.h"),
                str(Path(args.root_folder) / "nifat32.c"),
                str(Path(args.root_folder) / "src/*"),
                str(Path(args.root_folder) / "std/*"),
            ],
            base_path=str(tests_path),
            test_path=str(test),
            debug=args.debug,
            creation=creation,
            compiler=args.compiler,
            include_path=str(Path(args.root_folder) / "include"),
            flags=_collect_compile_flags(args),
            output_suffix=output_suffix,
        ))

    return test_bins


def _compile_selected_tests(
    args: argparse.Namespace,
    creation: bool = True,
    output_suffix: str = "",
) -> tuple[list[Path], list[Path]]:
    test_files = _get_selected_test_files(args)
    return test_files, _compile_test_files(args, test_files, creation=creation, output_suffix=output_suffix)


def _cleanup_test_bins(test_bins: list[Path]) -> None:
    logger.info("Cleaning up test binaries...")
    for bin_path in test_bins:
        try:
            bin_path.unlink(missing_ok=True)
            logger.info(f"Removed binary: {bin_path}")
        except Exception as e:
            logger.warning(f"Failed to remove {bin_path}: {e}")


def _sanitize_output(text: str | None) -> str:
    if not text:
        return ""
    return text.replace("\r", "").replace("\n", r"\\n").strip()


def _run_single_test(bin_path: Path, test_size: int, capture_output: bool = False) -> dict:
    logger.info(f"Running test: {bin_path.name} with size={test_size}")
    started = time.perf_counter()
    result = subprocess.run(
        [str(bin_path), str(test_size)],
        check=False,
        capture_output=capture_output,
        text=capture_output,
    )
    elapsed = time.perf_counter() - started

    stdout = result.stdout if capture_output else ""
    stderr = result.stderr if capture_output else ""
    if capture_output:
        if stdout:
            sys.stdout.write(stdout)
        if stderr:
            sys.stderr.write(stderr)

    success = result.returncode == 0
    if success:
        logger.info(f"Test {bin_path.name} exited with code {result.returncode}.")
    else:
        logger.error(f"Test {bin_path.name} failed with code {result.returncode}.")

    return {
        "test": bin_path.name,
        "returncode": result.returncode,
        "elapsed_sec": elapsed,
        "stdout": stdout,
        "stderr": stderr,
        "success": success,
    }


def run_tests(test_bins: list[Path], test_size: int) -> bool:
    all_ok = True
    for bin_path in test_bins:
        record = _run_single_test(bin_path=bin_path, test_size=test_size, capture_output=False)
        if not record["success"]:
            all_ok = False
            break
    return all_ok


def run_tests_detailed(test_bins: list[Path], test_size: int) -> dict:
    records: list[dict] = []
    for bin_path in test_bins:
        record = _run_single_test(bin_path=bin_path, test_size=test_size, capture_output=False)
        records.append(record)
        if not record["success"]:
            return {
                "success": False,
                "records": records,
                "failed_test": record["test"],
                "failed_returncode": record["returncode"],
            }

    return {
        "success": True,
        "records": records,
        "failed_test": None,
        "failed_returncode": 0,
    }


def run_tests_collect(test_bins: list[Path], test_size: int) -> list[dict]:
    records: list[dict] = []
    for bin_path in test_bins:
        records.append(_run_single_test(bin_path=bin_path, test_size=test_size, capture_output=True))
    return records


def _append_collection_records(output_path: str, collect_param: str, value: int, records: list[dict]) -> None:
    with open(output_path, "a", encoding="utf-8") as f:
        for record in records:
            line = (
                f"param={collect_param} "
                f"value={value} "
                f"test={record['test']} "
                f"rc={record['returncode']} "
                f"elapsed_sec={record['elapsed_sec']:.6f} "
                f"stdout={_sanitize_output(record['stdout'])} "
                f"stderr={_sanitize_output(record['stderr'])}"
            )
            f.write(line + "\n")


def collect_data(args: argparse.Namespace) -> None:
    if not args.collect_param:
        return

    target_attr = COLLECTABLE_ARGS[args.collect_param]
    output_path = args.collect_output

    if not args.collect_append:
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(
                f"# collect_param={args.collect_param} from={args.collect_from} to={args.collect_to} step={args.collect_step}\n"
            )

    for value in range(args.collect_from, args.collect_to + 1, args.collect_step):
        local_args = copy.deepcopy(args)
        setattr(local_args, target_attr, value)

        logger.info(f"Collect iteration: {args.collect_param}={value}")

        if local_args.new_image:
            _build_nifat32_image_base(
                formatter=local_args.formatter,
                clean=local_args.clean,
                spc=local_args.spc,
                v_size=local_args.image_size,
                bs_count=local_args.bs_count,
                fc=local_args.fat_count,
                jc=local_args.j_count,
                ec=local_args.e_count,
                b_bs_count=local_args.b_bs_count,
            )

        _, test_bins = _compile_selected_tests(local_args)
        records = run_tests_collect(test_bins=test_bins, test_size=local_args.test_size)
        _append_collection_records(
            output_path=output_path,
            collect_param=args.collect_param,
            value=value,
            records=records,
        )

        if local_args.clean:
            _cleanup_test_bins(test_bins)


def _build_image_from_args(args: argparse.Namespace, output: str = "nifat32.img") -> str:
    if not args.formatter:
        logger.error("Formatter path is required to build a new image.")
        raise SystemExit(1)

    return _build_nifat32_image_base(
        formatter=args.formatter,
        clean=args.clean,
        spc=args.spc,
        v_size=args.image_size,
        bs_count=args.bs_count,
        fc=args.fat_count,
        jc=args.j_count,
        ec=args.e_count,
        b_bs_count=args.b_bs_count,
        output=output,
    )


def _snapshot_path_for(image_path: str) -> str:
    image = Path(image_path)
    return str(image.with_name(f"{image.stem}.clean{image.suffix}"))


def _prepare_clean_snapshot(args: argparse.Namespace, image_path: str) -> str:
    image = Path(image_path)
    if args.new_image:
        _build_image_from_args(args, output=image.name)
    elif not image.exists():
        logger.error(f"Image does not exist: {image_path}. Use --new-image or place the image in the working directory.")
        raise SystemExit(1)

    snapshot_path = _snapshot_path_for(image_path)
    shutil.copyfile(image_path, snapshot_path)
    logger.info(f"Prepared clean snapshot: {snapshot_path}")
    return snapshot_path


def _restore_image(snapshot_path: str, image_path: str) -> None:
    shutil.copyfile(snapshot_path, image_path)
    logger.info(f"Image restored from snapshot {snapshot_path} -> {image_path}")


def _parse_scenario_line(line: str) -> tuple[str, int] | None:
    parts = line.split()
    if not parts:
        return None

    action = parts[0]
    if action == "C" and len(parts) == 1:
        return action, 0

    if len(parts) != 2 or action not in {"T", "I", "S", "C"}:
        return None

    return action, int(parts[1])


def _load_bitflip_scenario(args: argparse.Namespace) -> list[tuple[str, int]]:
    scenario: list[tuple[str, int]] = []

    if args.injector_scenario:
        logger.info(f"Injector scenario path: {args.injector_scenario}")
        try:
            with open(args.injector_scenario, "r", encoding="utf-8") as f:
                for raw_line in f:
                    line = raw_line.strip()
                    if not line or line.startswith("#"):
                        continue

                    parsed = _parse_scenario_line(line)
                    if parsed is None:
                        logger.warning(f"Ignoring invalid scenario line: {line}")
                        continue

                    scenario.append(parsed)
        except Exception as e:
            logger.error(f"Failed to load scenario: {e}")

    if not scenario:
        scenario = [("T", args.test_size), ("I", args.bitflips_count), ("T", args.test_size)]
        logger.warning(f"Using default fallback scenario={scenario}.")

    return scenario


def _append_bitflip_run_record(output_path: str, result: dict) -> None:
    with open(output_path, "a", encoding="utf-8") as f:
        line = (
            f"run={result['run']} "
            f"success={int(result['success'])} "
            f"failed_action={result['failed_action']} "
            f"failed_step={result['failed_step']} "
            f"failed_test={result['failed_test']} "
            f"failed_rc={result['failed_returncode']} "
            f"bitflips={result['bitflips']}"
        )
        f.write(line + "\n")


def _write_bitflip_summary(output_path: str, results: list[dict], scenario: list[tuple[str, int]]) -> None:
    total_runs = len(results)
    failed_runs = sum(1 for item in results if not item["success"])
    failure_rate = (failed_runs / total_runs * 100.0) if total_runs else 0.0

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(f"# scenario={scenario}\n")
        for result in results:
            line = (
                f"run={result['run']} "
                f"success={int(result['success'])} "
                f"failed_action={result['failed_action']} "
                f"failed_step={result['failed_step']} "
                f"failed_test={result['failed_test']} "
                f"failed_rc={result['failed_returncode']} "
                f"bitflips={result['bitflips']}"
            )
            f.write(line + "\n")
        f.write(
            f"summary total_runs={total_runs} failed_runs={failed_runs} failure_rate={failure_rate:.2f}%\n"
        )


def _run_bitflip_single(
    args: argparse.Namespace,
    image_path: str,
    clean_snapshot_path: str,
    scenario: list[tuple[str, int]],
    creation_bins: list[Path],
    no_creation_bins: list[Path],
    run_index: int,
) -> dict:
    from injector import random_bitflips, scratch_emulation

    logger.info(f"Starting bitflip run {run_index}/{args.bitflip_runs}")
    _restore_image(clean_snapshot_path, image_path)

    bitflips_count: float = 0.0
    current_bins = creation_bins
    creation_mode = True

    for step_index, (act, size) in enumerate(scenario, start=1):
        logger.info(f"Running scenario step={step_index}, act={act}, total_bitflips={bitflips_count}")

        if act == "T":
            logger.info(f"Forward testing, size={size}, creation_mode={creation_mode}...")
            test_result = run_tests_detailed(test_bins=current_bins, test_size=size)
            if not test_result["success"]:
                logger.error(
                    f"Bitflip run failed. run={run_index}, step={step_index}, bitflips={bitflips_count}, "
                    f"failed_test={test_result['failed_test']}"
                )
                return {
                    "run": run_index,
                    "success": False,
                    "failed_action": act,
                    "failed_step": step_index,
                    "failed_test": test_result["failed_test"],
                    "failed_returncode": test_result["failed_returncode"],
                    "bitflips": bitflips_count,
                }

            if creation_mode:
                logger.info("Switching to NO_CREATION binaries for subsequent checks on existing entries.")
                current_bins = no_creation_bins
                creation_mode = False
        elif act == "I":
            bitflips_count += size
            logger.info(f"Bitflip injection, size={size}...")
            random_bitflips(file_path=image_path, num_flips=size)
        elif act == "S":
            scratch_delta = size * args.scratch_width * args.scratch_intensity
            bitflips_count += scratch_delta
            logger.info(f"Scratch injection, size={size}, effective_bitflips={scratch_delta}...")
            scratch_emulation(
                file_path=image_path,
                scratch_length=size,
                width=args.scratch_width,
                intensity=args.scratch_intensity,
            )
        elif act == "C":
            logger.info("Cleaning image via snapshot restore.")
            _restore_image(clean_snapshot_path, image_path)
            current_bins = creation_bins
            creation_mode = True
        else:
            logger.warning(f"Incorrect scenario action! act={act}")

    return {
        "run": run_index,
        "success": True,
        "failed_action": "-",
        "failed_step": 0,
        "failed_test": "-",
        "failed_returncode": 0,
        "bitflips": bitflips_count,
    }


def _run_bitflip_campaign(args: argparse.Namespace, image_path: str) -> bool:
    if not args.tests_folder or not args.root_folder:
        logger.error("Bitflip mode requires --tests-folder and --root-folder.")
        raise SystemExit(1)

    scenario = _load_bitflip_scenario(args)
    selected_tests = _get_selected_test_files(args)
    if not selected_tests:
        logger.error("No tests selected for execution.")
        raise SystemExit(1)

    clean_snapshot_path = _prepare_clean_snapshot(args, image_path)
    creation_bins = _compile_test_files(args, selected_tests, creation=True, output_suffix=".create")
    no_creation_bins = _compile_test_files(args, selected_tests, creation=False, output_suffix=".nocreate")

    results: list[dict] = []
    try:
        for run_index in range(1, args.bitflip_runs + 1):
            result = _run_bitflip_single(
                args=args,
                image_path=image_path,
                clean_snapshot_path=clean_snapshot_path,
                scenario=scenario,
                creation_bins=creation_bins,
                no_creation_bins=no_creation_bins,
                run_index=run_index,
            )
            results.append(result)

            if args.bitflip_stats_output:
                if run_index == 1:
                    with open(args.bitflip_stats_output, "w", encoding="utf-8") as f:
                        f.write(f"# scenario={scenario}\n")
                _append_bitflip_run_record(args.bitflip_stats_output, result)

            if args.bitflip_runs == 1 and not result["success"]:
                break

        total_runs = len(results)
        failed_runs = sum(1 for item in results if not item["success"])
        failure_rate = (failed_runs / total_runs * 100.0) if total_runs else 0.0

        logger.info(
            f"Bitflip summary: total_runs={total_runs}, failed_runs={failed_runs}, failure_rate={failure_rate:.2f}%"
        )

        if args.bitflip_stats_output:
            _write_bitflip_summary(args.bitflip_stats_output, results, scenario)
            logger.info(f"Bitflip statistics saved to {args.bitflip_stats_output}")

        _restore_image(clean_snapshot_path, image_path)
        return failed_runs == 0
    finally:
        _cleanup_test_bins(creation_bins + no_creation_bins)
        if args.clean:
            try:
                Path(clean_snapshot_path).unlink(missing_ok=True)
            except Exception as e:
                logger.warning(f"Failed to remove clean snapshot {clean_snapshot_path}: {e}")


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
    parser.add_argument("--test-specific", type=str, default=None, help="Select the specific test.")

    parser.add_argument("--debug", nargs="+", help="Debug flags (e.g. debug, log, info, error, special)")
    parser.add_argument("--test-type", type=str, default="default", help="Test type: default, bitflip.")
    parser.add_argument("--new-image", action="store_true", help="Build a new nifat32 image for test.")
    parser.add_argument("--clean", action="store_true", help="Clean directory from build files and binary files.")
    parser.add_argument("--test-size", type=int, default=1000, help="Test size.")
    parser.add_argument("--compiler", type=str, default="gcc-14", help="Compiler for compilation.")

    parser.add_argument("--formatter", type=str, help="Path to formatter tool.")
    parser.add_argument("--tests-folder", type=str, help="Path to tests folder. (Test name example: test_*.c).")
    parser.add_argument("--root-folder", type=str, help="Path to NIFAT32 root folder (With nifat32.h).")
    parser.add_argument("--image-size", type=int, default=64, help="Image size on MB")
    parser.add_argument("--spc", type=int, default=8, help="Sectors per cluster")
    parser.add_argument("--bs-count", type=int, default=5, help="Bootsector count")
    parser.add_argument("--b-bs-count", type=int, default=0, help="Broken bootsector count")
    parser.add_argument("--j-count", type=int, default=0, help="Journals count")
    parser.add_argument("--e-count", type=int, default=0, help="Error-code count")
    parser.add_argument("--fat-count", type=int, default=5, help="FAT count")

    parser.add_argument("--collect-param", choices=sorted(COLLECTABLE_ARGS.keys()), help="Parameter to sweep and collect.")
    parser.add_argument("--collect-from", type=int, default=1, help="Sweep start value.")
    parser.add_argument("--collect-to", type=int, default=1, help="Sweep end value.")
    parser.add_argument("--collect-step", type=int, default=1, help="Sweep step.")
    parser.add_argument("--collect-output", type=str, default="collection.txt", help="Output txt file.")
    parser.add_argument("--collect-append", action="store_true", help="Append to output file instead of truncating it.")

    parser.add_argument("--bitflips-strategy", type=str, default="random", help="Bitflip strategy: random or scratch.")
    parser.add_argument("--scratch-width", type=int, default=1)
    parser.add_argument("--scratch-intensity", type=float, default=.7)
    parser.add_argument("--bitflips-count", type=int, default=10)
    parser.add_argument("--bitflip-runs", type=int, default=1, help="Repeat the whole bitflip scenario N times and collect failure rate.")
    parser.add_argument("--bitflip-stats-output", type=str, default=None, help="Optional txt file for per-run bitflip statistics.")

    parser.add_argument("--injector-scenario", type=str, help="Path to injector scenario.")
    args = parser.parse_args()

    if len(sys.argv) == 1:
        print_welcome(parser=parser)
        raise SystemExit(1)

    logger.info(f"Test starting. Type={args.test_type}")
    if args.test_type not in ["benchmark", "index", "default", "bitflip"]:
        logger.error(f"Unknown test type: {args.test_type}")
        raise SystemExit(1)

    logger.info(f"Test size: {args.test_size}")
    if args.clean:
        logger.info("Temporary build files and binary files will be cleaned after the test.")

    if args.debug:
        logger.info(f"Debug flags: {args.debug}")

    if args.collect_param:
        collect_data(args)
        raise SystemExit(0)

    image_path = "nifat32.img"
    test_bins: list[Path] = []

    if args.test_type == "bitflip":
        logger.info("Bitflip testing mode selected.")
        logger.info(f"Bitflip strategy: {args.bitflips_strategy}")
        logger.info(f"Bitflips count:   {args.bitflips_count}")
        logger.info(f"Bitflip runs:     {args.bitflip_runs}")

        if args.bitflips_strategy == "scratch":
            logger.info(f"Scratch width: {args.scratch_width}")
            logger.info(f"Scratch intensity: {args.scratch_intensity}")

        success = _run_bitflip_campaign(args, image_path=image_path)
        raise SystemExit(0 if success else 1)

    if args.new_image:
        logger.info("A new NIFAT32 image will be created for the test.")
        logger.info(f"Image size:       {args.image_size} MB")
        logger.info(f"Bootsector count: {args.bs_count}")
        logger.info(f"Broken bs count:  {args.b_bs_count}")
        logger.info(f"Journal count:    {args.j_count}")
        logger.info(f"Error-code count: {args.e_count}")
        logger.info(f"FAT count:        {args.fat_count}")
        if args.formatter:
            logger.info(f"Formatter tool path: {args.formatter}")

        image_path = _build_image_from_args(args, output=image_path)

    if args.tests_folder and args.root_folder:
        logger.info(f"Tests folder path:        {args.tests_folder}")
        logger.info(f"NIFAT32 root folder path: {args.root_folder}")
        _, test_bins = _compile_selected_tests(args)

    logger.info("Running compiled tests sequentially...")
    success = run_tests(test_bins=test_bins, test_size=args.test_size)

    if args.clean:
        _cleanup_test_bins(test_bins)

    raise SystemExit(0 if success else 1)
