import argparse
import glob
import logging
import shutil
import subprocess
import time
from pathlib import Path

def configure_logging(verbose: bool) -> None:
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s | %(levelname)s | %(message)s",
        datefmt="%H:%M:%S",
    )

def run_cmd(cmd: list[str], cwd: str | None = None) -> None:
    logging.debug("RUN: %s", " ".join(cmd))
    subprocess.run(cmd, cwd=cwd, check=True)

def build_image(args: argparse.Namespace) -> Path:
    formatter_dir = Path(args.formatter).resolve()
    formatter_bin = formatter_dir / "formatter"

    output_image = Path(args.image_path).resolve()
    output_name = output_image.name

    logging.info("Building the formatter in %s", formatter_dir)
    run_cmd(["make"], cwd=str(formatter_dir))

    cmd = [
        str(formatter_bin),
        "-o", output_name,
        "-s", "nifat32",
        "--volume-size", str(args.image_size),
        "--spc", str(args.spc),
        "--fc", str(args.fat_count),
        "--bsbc", str(args.bs_count),
        "--b-bsbc", str(args.b_bs_count),
        "--jc", str(args.j_count),
        "--ec", str(args.e_count),
    ]

    logging.info("Image generation %s", output_name)
    run_cmd(cmd, cwd=str(formatter_dir))

    produced = formatter_dir / output_name
    if not produced.exists():
        raise SystemExit(f"Image wasn't created: {produced}")

    shutil.copyfile(produced, output_image)
    logging.info("Image ready: %s", output_image)

    return output_image

def compile_flags(args: argparse.Namespace) -> list[str]:
    return [
        f"-I{Path(args.root_folder) / 'include'}",
        f"-I{Path(args.tests_folder)}",
        f"-DV_SIZE={args.image_size}",
        f"-DBS_COUNT={args.bs_count}",
        f"-DB_BS_COUNT={args.b_bs_count}",
        f"-DJ_COUNT={args.j_count}",
        f"-DE_COUNT={args.e_count}",
        f"-DFC_COUNT={args.fat_count}",
        f"-DSPC={args.spc}",
    ]

def compile_test_binary(
    args: argparse.Namespace,
    source_name: str,
    output_name: str,
    creation: bool,
) -> Path:
    tests_dir = Path(args.tests_folder).resolve()
    root_dir = Path(args.root_folder).resolve()

    source_path = tests_dir / source_name
    output_path = tests_dir / output_name
    nifat32_c = root_dir / "nifat32.c"

    src_files = sorted(glob.glob(str(root_dir / "src" / "*.c")))
    std_files = sorted(glob.glob(str(root_dir / "std" / "*.c")))

    cmd = [
        args.compiler,
        *compile_flags(args),
    ]

    if not creation:
        cmd.append("-DNO_CREATION")

    cmd.extend([
        str(source_path),
        str(nifat32_c),
        *src_files,
        *std_files,
        "-o", str(output_path),
    ])

    logging.info(
        "Compilation of the %s -> %s (%s)",
        source_name,
        output_path.name,
        "creation" if creation else "no_creation",
    )
    run_cmd(cmd)

    return output_path

def run_binary(binary_path: Path, size_arg: int | None) -> dict:
    cmd = [str(binary_path)]
    if size_arg is not None:
        cmd.append(str(size_arg))

    logging.info("Running the %s", binary_path.name)
    started = time.perf_counter()
    result = subprocess.run(cmd, check=False)
    elapsed = time.perf_counter() - started

    success = result.returncode == 0
    if success:
        logging.info("%s Success: %.3fs", binary_path.name, elapsed)
    else:
        logging.error("%s Error with the %s code", binary_path.name, result.returncode)

    return {
        "binary": binary_path.name,
        "returncode": result.returncode,
        "elapsed_sec": elapsed,
        "success": success,
    }

def ensure_image_and_snapshot(args: argparse.Namespace) -> tuple[Path, Path]:
    image_path = Path(args.image_path).resolve()

    if args.new_image:
        image_path = build_image(args)
    elif not image_path.exists():
        raise SystemExit(
            f"Image isn't found: {image_path}."
        )

    snapshot_path = image_path.with_name(f"{image_path.stem}.clean{image_path.suffix}")
    shutil.copyfile(image_path, snapshot_path)
    logging.info("Snapshot: %s is ready", snapshot_path)

    return image_path, snapshot_path

def restore_image(snapshot_path: Path, image_path: Path) -> None:
    shutil.copyfile(snapshot_path, image_path)
    logging.info("Image restored from the snapshot")

def inject_errors(args: argparse.Namespace, image_path: Path) -> float:
    from injector import random_bitflips, scratch_emulation

    if args.bitflips_strategy == "random":
        logging.info("Injectio: random bitflips: %s", args.bitflips_count)
        random_bitflips(file_path=str(image_path), num_flips=args.bitflips_count)
        return float(args.bitflips_count)

    if args.bitflips_strategy == "scratch":
        logging.info(
            "Injection: scratch: length=%s width=%s intensity=%s",
            args.bitflips_count,
            args.scratch_width,
            args.scratch_intensity,
        )
        scratch_emulation(
            file_path=str(image_path),
            scratch_length=args.bitflips_count,
            width=args.scratch_width,
            intensity=args.scratch_intensity,
        )
        return float(args.bitflips_count) * args.scratch_width * args.scratch_intensity

def write_stats(output_path: str, results: list[dict]) -> None:
    total_runs = len(results)
    failed_runs = sum(1 for r in results if not r["success"])
    failure_rate = (failed_runs / total_runs * 100.0) if total_runs else 0.0

    with open(output_path, "w", encoding="utf-8") as f:
        for r in results:
            f.write(
                "run={run} success={success} stage={stage} "
                "setup_rc={setup_rc} test_rc={test_rc} "
                "setup_time={setup_time:.6f} test_time={test_time:.6f} "
                "bitflips={bitflips}\n".format(
                    run=r["run"],
                    success=int(r["success"]),
                    stage=r["failed_stage"],
                    setup_rc=r["setup_rc"],
                    test_rc=r["test_rc"],
                    setup_time=r["setup_time"],
                    test_time=r["test_time"],
                    bitflips=r["bitflips"],
                )
            )

        f.write(
            f"summary total_runs={total_runs} failed_runs={failed_runs} "
            f"failure_rate={failure_rate:.2f}%\n"
        )

    logging.info("Result saved in the %s", output_path)

def cleanup_files(paths: list[Path]) -> None:
    for path in paths:
        try:
            path.unlink(missing_ok=True)
            logging.debug("Cleanup the file %s", path)
        except Exception as e:
            logging.warning("Can't clean the %s: %s", path, e)

def run_campaign(args: argparse.Namespace) -> int:
    image_path, snapshot_path = ensure_image_and_snapshot(args)

    setup_bin = compile_test_binary(
        args=args,
        source_name=args.setup_source,
        output_name="test_nifat32_bitflip_setup.bin",
        creation=True,
    )
    test_bin = compile_test_binary(
        args=args,
        source_name=args.test_source,
        output_name="test_nifat32_bitflip_test.bin",
        creation=False,
    )

    results: list[dict] = []

    try:
        for run_idx in range(1, args.runs + 1):
            logging.info("========== RUN %s / %s ==========", run_idx, args.runs)

            restore_image(snapshot_path, image_path)
            setup_result = run_binary(setup_bin, args.setup_size)
            if not setup_result["success"]:
                results.append({
                    "run": run_idx,
                    "success": False,
                    "failed_stage": "setup",
                    "setup_rc": setup_result["returncode"],
                    "test_rc": -1,
                    "setup_time": setup_result["elapsed_sec"],
                    "test_time": 0.0,
                    "bitflips": 0.0,
                })
                continue

            injected = inject_errors(args, image_path)

            test_result = run_binary(test_bin, args.test_size)
            results.append({
                "run": run_idx,
                "success": test_result["success"],
                "failed_stage": "-" if test_result["success"] else "test",
                "setup_rc": setup_result["returncode"],
                "test_rc": test_result["returncode"],
                "setup_time": setup_result["elapsed_sec"],
                "test_time": test_result["elapsed_sec"],
                "bitflips": injected,
            })

        failed_runs = sum(1 for r in results if not r["success"])
        failure_rate = failed_runs / len(results) * 100.0 if results else 0.0

        logging.info(
            "Complete: total_runs=%s failed_runs=%s failure_rate=%.2f%%",
            len(results),
            failed_runs,
            failure_rate,
        )

        if args.stats_output:
            write_stats(args.stats_output, results)

        restore_image(snapshot_path, image_path)
        return 0 if failed_runs == 0 else 1

    finally:
        if args.cleanup:
            cleanup_files([setup_bin, test_bin, snapshot_path])

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Bitflip tester!")
    parser.add_argument("--tests-folder", required=True)
    parser.add_argument("--root-folder", required=True)
    parser.add_argument("--formatter")
    parser.add_argument("--image-path")

    parser.add_argument(
        "--setup-source",
        default="test_nifat32_bitflip_setup.c"
    )
    parser.add_argument(
        "--test-source",
        default="test_nifat32_bitflip_test.c"
    )

    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--new-image", action="store_true")
    parser.add_argument("--cleanup", action="store_true")
    parser.add_argument("--verbose", action="store_true")

    parser.add_argument("--runs", type=int, default=100)
    parser.add_argument("--setup-size", type=int, default=1000)
    parser.add_argument("--test-size", type=int, default=1000)
    parser.add_argument("--stats-output", default="bitflip_stats.txt")

    parser.add_argument(
        "--bitflips-strategy",
        choices=["random", "scratch"],
        default="random"
    )
    parser.add_argument("--bitflips-count", type=int, default=50)
    parser.add_argument("--scratch-width", type=int, default=1)
    parser.add_argument("--scratch-intensity", type=float, default=0.7)

    parser.add_argument("--image-size", type=int, default=64)
    parser.add_argument("--spc", type=int, default=8, help="Sectors per cluster")
    parser.add_argument("--bs-count", type=int, default=5, help="Bootsector count")
    parser.add_argument("--b-bs-count", type=int, default=0, help="Broken bootsector count")
    parser.add_argument("--j-count", type=int, default=0, help="Journal count")
    parser.add_argument("--e-count", type=int, default=0, help="Error-code count")
    parser.add_argument("--fat-count", type=int, default=5, help="FAT count")

    return parser.parse_args()

def main() -> int:
    args = parse_args()
    configure_logging(args.verbose)
    return run_campaign(args)

if __name__ == "__main__":
    raise SystemExit(main())
