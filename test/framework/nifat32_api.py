from __future__ import annotations
from typing import Any, Callable
from api.fs_api import FSapi, File

def to_83(filename: str | None) -> str | None:
    if not filename:
        return None
    
    name, ext = os.path.splitext(os.path.basename(filename))
    name = name.upper()[:8].ljust(8)
    ext = ext.upper().lstrip('.')[:3].ljust(3)
    return name + ext

import os
import ctypes
from ctypes import *

class NIFAT32file(File):
    def __init__(self, ci: int) -> None:
        super().__init__()
        self.ci = ci

READ_SECTOR_FUNC = CFUNCTYPE(c_int, c_uint32, c_uint32, POINTER(c_char), c_int)
WRITE_SECTOR_FUNC = CFUNCTYPE(c_int, c_uint32, c_uint32, POINTER(c_char), c_int)
FPRINTF_FUNC = CFUNCTYPE(c_int, c_char_p)
VPRINTF_FUNC = CFUNCTYPE(c_int, c_char_p, c_void_p)

class DiskIO(Structure):
    _fields_ = [
        ("read_sector", READ_SECTOR_FUNC),
        ("write_sector", WRITE_SECTOR_FUNC),
        ("sector_size", c_int)
    ]
    
    @staticmethod
    def new(
        read_sector_func: Callable[[int, int, Any, int], int],
        write_sector_func: Callable[[int, int, Any, int], int],
        sector_size: int        
    ) -> DiskIO:
        disk = DiskIO()
        disk.read_sector = read_sector_func
        disk.write_sector = write_sector_func
        disk.sector_size = sector_size
        return disk

class LogIO(Structure):
    _fields_ = [
        ("fd_fprintf", FPRINTF_FUNC),
        ("fd_vfprintf", VPRINTF_FUNC),
    ]
    
    @staticmethod
    def new(
        fd_fprintf_func: Callable[[bytes], int],
        fd_vfprintf_func: Callable[[bytes, Any], int]        
    ) -> LogIO:
        log = LogIO()
        log.fd_fprintf = fd_fprintf_func
        log.fd_vfprintf = fd_vfprintf_func
        return log

class NIFAT32Params(Structure):
    _fields_ = [
        ("fat_cache", c_char),
        ("bs_num", c_char),
        ("bs_count", c_char),
        ("ts", c_uint32),
        ("jc", c_ubyte),
        ("disk_io", DiskIO),
        ("logg_io", LogIO)
    ]
    
    @staticmethod
    def new(
        fat_cache: bytes,
        bs_num: bytes,
        bs_count: bytes,
        ts: int,
        jc: int,
        disk_io: DiskIO,
        logg_io: LogIO
    ) -> NIFAT32Params:
        params = NIFAT32Params()
        params.fat_cache = fat_cache
        params.bs_num = bs_num
        params.bs_count = bs_count
        params.ts = ts
        params.jc = jc
        params.disk_io = disk_io
        params.logg_io = logg_io
        return params

class CInfo(Structure):
    _fields_ = [
        ("full_name", c_char * 12),
        ("file_name", c_char * 8),
        ("file_extension", c_char * 4),
        ("size", c_uint32),
        ("type", c_int),
    ]
    
    @staticmethod
    def new(name: str, ext: str | None, is_dir: bool, size: int) -> CInfo:
        info = CInfo()
        ext = ext or ""

        fatname: str = to_83(name + ext)
        info.full_name = fatname.encode("ascii")[:12].ljust(12, b'\x00')
        info.file_name = name.encode("ascii")[:8].ljust(8, b'\x00')
        info.file_extension = ext.encode("ascii")[:3].ljust(3, b'\x00')
        info.size = 1 if not is_dir else 0
        info.type = size if not is_dir else 0
        return info

from enum import IntFlag

class OpenMode(IntFlag):
    READ = 0b0001
    WRITE = 0b0010
    CREATE = 0b0100

class TargetType(IntFlag):
    NONE = 0b0000
    FILE = 0b0001
    DIR  = 0b0010

class NIFAT32api(FSapi):
    def __init__(self, lib: str, params: NIFAT32Params) -> None:
        super().__init__()
        self.lib: ctypes.CDLL = ctypes.CDLL(lib)

        self.lib.NIFAT32_init.argtypes = [POINTER(NIFAT32Params)]
        self.lib.NIFAT32_init.restype = c_int

        self.lib.NIFAT32_repair_bootsectors.argtypes = []
        self.lib.NIFAT32_repair_bootsectors.restype = c_int

        self.lib.NIFAT32_unload.argtypes = []
        self.lib.NIFAT32_unload.restype = c_int

        self.lib.NIFAT32_content_exists.argtypes = [c_char_p]
        self.lib.NIFAT32_content_exists.restype = c_int

        self.lib.NIFAT32_open_content.argtypes = [c_int, c_char_p, ctypes.c_ubyte]
        self.lib.NIFAT32_open_content.restype = c_int

        self.lib.NIFAT32_stat_content.argtypes = [c_int, POINTER(CInfo)]
        self.lib.NIFAT32_stat_content.restype = c_int

        self.lib.NIFAT32_change_meta.argtypes = [c_int, POINTER(CInfo)]
        self.lib.NIFAT32_change_meta.restype = c_int

        self.lib.NIFAT32_read_content2buffer.argtypes = [c_int, c_uint32, POINTER(c_char), c_int]
        self.lib.NIFAT32_read_content2buffer.restype = c_int

        self.lib.NIFAT32_write_buffer2content.argtypes = [c_int, c_uint32, POINTER(c_char), c_int]
        self.lib.NIFAT32_write_buffer2content.restype = c_int

        self.lib.NIFAT32_truncate_content.argtypes = [c_int, c_uint32, c_int]
        self.lib.NIFAT32_truncate_content.restype = c_int

        self.lib.NIFAT32_index_content.argtypes = [c_int]
        self.lib.NIFAT32_index_content.restype = c_int

        self.lib.NIFAT32_close_content.argtypes = [c_int]
        self.lib.NIFAT32_close_content.restype = c_int

        self.lib.NIFAT32_put_content.argtypes = [c_int, POINTER(CInfo), c_int]
        self.lib.NIFAT32_put_content.restype = c_int

        self.lib.NIFAT32_copy_content.argtypes = [c_int, c_int, ctypes.c_char]
        self.lib.NIFAT32_copy_content.restype = c_int

        self.lib.NIFAT32_delete_content.argtypes = [c_int]
        self.lib.NIFAT32_delete_content.restype = c_int

        self.lib.NIFAT32_repair_content.argtypes = [c_int, c_int]
        self.lib.NIFAT32_repair_content.restype = c_int
        
        res = self.lib.NIFAT32_init(byref(params))
        if res != 1:
            raise RuntimeError(f"NIFAT32_init failed with code {res}")

    @staticmethod
    def get_open_mode(mode: OpenMode, target: TargetType) -> int:
        return ((target & 0b1111) << 4) | (mode & 0b1111)

    def open(self, **kwargs) -> File:
        return NIFAT32file(
            ci=self.lib.NIFAT32_open_content(kwargs.get("rci", -1), kwargs.get("path", None), kwargs.get("mode", 0))
        )

    def close(self, file: File) -> bool:
        if isinstance(file, NIFAT32file):
            self.lib.NIFAT32_close_content(file.ci)
            return True
        
        return False
    
    def mkfile(self, **kwargs) -> bool:
        name: str = kwargs.get("name", "")
        ext: str = kwargs.get("ext", "")
        location: str = to_83(kwargs.get("location", None))
        rt: NIFAT32file = self.open(path=location)
        self.lib.NIFAT32_put_content(rt.ci, byref(CInfo.new(name=name, ext=ext, size=1, is_dir=False)), 1)
        return True
    
    def mkdir(self, **kwargs) -> bool:
        name: str = kwargs.get("name", "")
        location: str = to_83(kwargs.get("location", None))
        rt: NIFAT32file = self.open(path=location)
        self.lib.NIFAT32_put_content(rt.ci, byref(CInfo.new(name=name, size=0, is_dir=True)), 1)
        return True
    
    def write(self, file: File, **kwargs) -> int:
        if isinstance(file, NIFAT32file):
            data = kwargs.get("data", b"")
            size = kwargs.get("size", len(data))
            c_buf = (c_char * size).from_buffer_copy(data)

            return self.lib.NIFAT32_write_buffer2content(
                file.ci, kwargs.get("offset", 0), c_buf, size
            )
            
        return 0
    
    def read(self, file: File, **kwargs) -> int:
        if isinstance(file, NIFAT32file):
            size = kwargs.get("size", 0)
            buffer = kwargs.get("buffer")
            if not buffer:
                buffer = (c_char * size)()

            return self.lib.NIFAT32_read_content2buffer(
                file.ci, kwargs.get("offset", 0), buffer, size
            )
            
        return 0
    
    def trunc(self, file: File, offset: int, size: int) -> bool:
        if isinstance(file, NIFAT32file):
            self.lib.NIFAT32_truncate_content(file.ci, offset, size)
            return True
        
        return False
    
    def rename(self, file: File, **kwargs) -> bool:
        if isinstance(file, NIFAT32file) and isinstance(kwargs.get("info", None), CInfo):
            self.lib.NIFAT32_change_meta(file.ci, kwargs.get("info", None))
            return True
        
        return False
    
    def delete(self, file: File) -> bool:
        if isinstance(file, NIFAT32file):
            self.lib.NIFAT32_delete_content(file.ci)
            return True
        
        return False
    
    def __del__(self) -> None:
        self.lib.NIFAT32_unload()

if __name__ == "__main__":
    disk_fd = None
    sector_size = 512

    @READ_SECTOR_FUNC
    def dummy_read(sector_addr, offset, buffer, buff_size):
        if buff_size == 0:
            return 1
        
        global disk_fd, sector_size
        try:
            data = os.pread(disk_fd, buff_size, sector_addr * sector_size + offset)
            if not data:
                return 0
            
            memmove(buffer, data, len(data))
            return 1
        except Exception as e:
            print(f"[API] Read error: {e}")
            return 0

    @WRITE_SECTOR_FUNC
    def dummy_write(sector_addr, offset, data_ptr, data_size):
        if data_size == 0:
            return 1
        
        global disk_fd, sector_size
        try:
            buf = string_at(data_ptr, data_size)
            written = os.pwrite(disk_fd, buf, sector_addr * sector_size + offset)
            return 1 if written == data_size else 0
        except Exception as e:
            print(f"[API] Write error: {e}")
            return 0

    import sys

    @FPRINTF_FUNC
    def dummy_fprintf(fmt, *args):
        try:
            fmt_str = fmt.decode('utf-8')
        except UnicodeDecodeError:
            fmt_str = fmt.decode('latin1')
        
        try:
            formatted = fmt_str % args if args else fmt_str
        except (TypeError, ValueError) as e:
            formatted = fmt_str
        
        sys.stdout.write(formatted)
        return len(formatted)

    @VPRINTF_FUNC
    def dummy_vfprintf(fmt, args):
        try:
            fmt_str = fmt.decode('utf-8')
        except UnicodeDecodeError:
            fmt_str = fmt.decode('latin1')
        
        try:
            formatted = fmt_str % args if args else fmt_str
        except (TypeError, ValueError) as e:
            formatted = fmt_str
        
        sys.stdout.write(formatted)
        return len(formatted)
    
    import argparse
    parser = argparse.ArgumentParser(description="Initialize NIFAT32 API with parameters.")
    parser.add_argument("--fat-cache", type=int, default=1, help="FAT cache value")
    parser.add_argument("--bs-count", type=int, default=6, help="Boot sector count")
    parser.add_argument("--jc", type=int, default=1, help="Jump count")
    parser.add_argument("--v-size", type=int, default=128, help="Volume size in mbytes")
    parser.add_argument("--sector-size", type=int, default=512, help="Sector size in bytes")
    parser.add_argument("--lib", type=str, required=True, help="Path to shared library")
    parser.add_argument("--image", type=str, required=True, help="Path to disk image")

    args = parser.parse_args()

    disk_fd = os.open(args.image, os.O_RDWR)
    sector_size = args.sector_size
    ts = (args.v_size * 1024 * 1024) // args.sector_size

    disk_io: DiskIO = DiskIO.new(
        read_sector_func=dummy_read,
        write_sector_func=dummy_write,
        sector_size=args.sector_size
    )

    log_io: LogIO = LogIO.new(
        fd_fprintf_func=dummy_fprintf,
        fd_vfprintf_func=dummy_vfprintf
    )

    params: NIFAT32Params = NIFAT32Params.new(
        fat_cache=args.fat_cache,
        bs_num=0,
        bs_count=args.bs_count,
        ts=ts,
        jc=args.jc,
        disk_io=disk_io,
        logg_io=log_io
    )

    api: NIFAT32api = NIFAT32api(lib=args.lib, params=params)
    print("[API] NIFAT32 API initialized successfully. Reading file...")
    
    tfile_name: str = to_83("test.txt")
    print(f"[API] Opening {tfile_name}...")
    tfile: NIFAT32file = api.open(path=None, mode=NIFAT32api.get_open_mode(OpenMode.READ, TargetType.NONE))
    buf = create_string_buffer(1024)
    read_bytes = api.read(file=tfile, offset=0, size=512, buffer=buf)
    data = buf.raw[:read_bytes]
    
    print(f"[API] Data: {data.hex()}, ci={tfile.ci}")
    api.close(file=tfile)
