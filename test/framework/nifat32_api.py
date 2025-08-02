from framework.fs_api import FSapi, File

import os
import ctypes
from ctypes import *

class NIFAT32file(File):
    def __init__(self, ci: int, fs: FSapi) -> None:
        super().__init__()
        self.ci = ci
        self.fs = fs

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

class LogIO(Structure):
    _fields_ = [
        ("fd_fprintf", FPRINTF_FUNC),
        ("fd_vfprintf", VPRINTF_FUNC),
    ]

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

class CInfo(Structure):
    _fields_ = [
        ("full_name", c_char * 12),
        ("file_name", c_char * 8),
        ("file_extension", c_char * 4),
        ("size", c_uint32),
        ("type", c_int),
    ]

class NIFAT32api(FSapi):
    def __init__(self, lib: str, params: NIFAT32Params) -> int:
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
        
        return self.lib.NIFAT32_init(byref(params))

    def open(self, **kwargs) -> File:
        return NIFAT32file(
            ci=self.lib.NIFAT32_open_content(kwargs.get("rci", -1), kwargs.get("path", None), kwargs.get("mode", 0)),
            fs=self
        )

    def close(self, file: File) -> bool:
        if isinstance(file, NIFAT32api):
            self.lib.NIFAT32_close_content(file.ci)
            return True
        
        return False
    
    def _to_83(filename: str | None) -> str | None:
        if not filename:
            return None
        
        name, ext = os.path.splitext(os.path.basename(filename))
        name = name.upper()[:8].ljust(8)
        ext = ext.upper().lstrip('.')[:3].ljust(3)
        return name + ext
    
    def mkfile(self, **kwargs) -> bool:
        name: str = kwargs.get("name", "")
        ext: str = kwargs.get("ext", "")
        
        location: str = self._to_83(kwargs.get("location", None))
        fatname: str = self._to_83(filename=name + ext)
        rt: NIFAT32file = self.open(path=location)
        
        self.lib.NIFAT32_put_content(rt.ci, CInfo(
            full_name=fatname, file_name=name, file_extension=ext, size=1, type=1), 1
        )
        
        return True
    
    def mkdir(self, **kwargs) -> bool:
        name: str = kwargs.get("name", "")
        location: str = self._to_83(kwargs.get("location", None))
        fatname: str = self._to_83(filename=name)
        rt: NIFAT32file = self.open(path=location)
        
        self.lib.NIFAT32_put_content(rt.ci, CInfo(
            full_name=fatname, file_name=name, size=0, type=2), 1
        )
        
        return True
