import os
import shutil
import subprocess

from pathlib import Path
from dataclasses import dataclass
from building.builder import ImageBuilder

@dataclass
class ImageParams:
    root_path: str
    formatter_path: str
    save_path: str
    spc: int
    v_size: int
    bs_count: int
    fc: int
    jc: int

@dataclass
class CompilerParams:
    debug: bool
    compiler: str
    clean: bool

class NIFAT32builder(ImageBuilder):
    def __init__(
        self, image: ImageParams, compiler: CompilerParams
    ) -> None:
        """NIFAT32 files builder.

        Args:
            rpath (str): Root path. Path to root folder of nifat32.
            fpath (str): Formatter path. Path to formatter tool.
            spath (str): Tmp location for builded binaries.
        """
        super().__init__()
        self.iparams: ImageParams = image
        self.cparams: CompilerParams = compiler
        
    def image_build(self) -> None:
        formatter_dir: Path = Path(self.iparams.formatter_path).resolve()
        formatter_bin: Path = formatter_dir / "formatter"
        output_file: Path = Path("nifat32.img")

        try:
            subprocess.run(["make"], cwd=formatter_dir, check=True)
            build_seq: list[str] = [
                str(formatter_bin),
                "-o", str(output_file),
                "-s", "nifat32",
                "--volume-size", str(self.iparams.v_size),
                "--spc", str(self.iparams.spc),
                "--fc", str(self.iparams.fc),
                "--bsbc", str(self.iparams.bs_count),
                "--jc", str(self.iparams.jc),
            ]

            subprocess.run(build_seq, cwd=formatter_dir, check=True)
            os.remove(str(formatter_bin))
            
            final_path: Path = Path(self.iparams.save_path).resolve()
            shutil.move(str(formatter_dir / output_file), str(final_path / "nifat32.img"))
        except subprocess.CalledProcessError as e:
            print(f"Error during subprocess: {e}")
        except FileNotFoundError as e:
            print(f"File not found: {e}")
        except Exception as e:
            print(f"Unexpected error: {e}")
    
    def so_build(self) -> None:
        fat_path: Path = Path(self.iparams.root_path) / "nifat32.c"
        std_path: Path = Path(self.iparams.root_path) / "std/*"
        src_path: Path = Path(self.iparams.root_path) / "src/*"
        build_flags: list[str] = [
            "-fPIC", "-shared", "-nostdlib", "-nodefaultlibs", "-Ikernel/include"
        ]
        
        if self.cparams.debug:
            build_flags.extend([
                "-DERROR_LOGS", "-DWARNING_LOGS", "-DDEBUG_LOGS", "-DINFO_LOGS", "-DLOGGING_LOGS"
            ])
        
        compile_cmd: list[str] = [
            self.cparams.compiler, *build_flags,
            str(fat_path), str(std_path), str(src_path),
            "-o", str(Path(self.iparams.save_path) / "nifat32.so")
        ]

        try:
            subprocess.run(" ".join(compile_cmd), shell=True, check=True)
        except subprocess.CalledProcessError as e:
            raise e
        
    def clean(self) -> None:
        pass

if __name__ == "__main__":
    """
    This is an example usage of NIFAT32builder class.
    In default workcase, this class should be used in testing framework.
    """
    import argparse

    parser = argparse.ArgumentParser(
        description="NIFAT32 test image and shared library builder"
    )

    # Image parameters
    parser.add_argument("--root-path", type=str, required=True, help="Path to the root directory to include in the image")
    parser.add_argument("--formatter-path", type=str, required=True, help="Path to the NIFAT32 formatter executable")
    parser.add_argument("--save-path", type=str, required=True, help="Where to save the output disk image")
    parser.add_argument("--spc", type=int, default=8, help="Sectors per cluster (default: 8)")
    parser.add_argument("--v-size", type=int, default=128, help="Virtual disk size in MB (default: 128)")
    parser.add_argument("--bs-count", type=int, default=10, help="Boot sector count (default: 10)")
    parser.add_argument("--fc", type=int, default=6, help="FAT cache size (default: 6)")
    parser.add_argument("--jc", type=int, default=1, help="Jump cluster count (default: 1)")

    # Compiler parameters
    parser.add_argument("--compiler", type=str, default="gcc-14", help="Compiler to use (default: gcc-14)")
    parser.add_argument("--debug", action="store_true", help="Enable debug mode")
    parser.add_argument("--no-clean", action="store_true", help="Do not clean temporary build files")

    args = parser.parse_args()

    builder: NIFAT32builder = NIFAT32builder(
        image=ImageParams(
            root_path=args.root_path,
            formatter_path=args.formatter_path,
            save_path=args.save_path,
            spc=args.spc,
            v_size=args.v_size,
            bs_count=args.bs_count,
            fc=args.fc,
            jc=args.jc,
        ),
        compiler=CompilerParams(
            debug=args.debug,
            compiler=args.compiler,
            clean=not args.no_clean
        )
    )

    builder.image_build()
    builder.so_build()
