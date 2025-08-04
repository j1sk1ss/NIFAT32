from __future__ import annotations
from typing import Any, Callable
from framework.api.fs_api import FSapi, File

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
        self.lib = ctypes.CDLL(lib)

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
