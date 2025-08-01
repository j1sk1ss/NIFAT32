from framework.fs_api import FSapi, File

import ctypes
from ctypes import c_int, c_uint32, c_char_p, c_char, POINTER, Structure, c_ubyte

class NIFAT32file(File):
    def __init__(self, ci: int, fs: FSapi) -> None:
        super().__init__()
        self.ci = ci
        self.fs = fs

class NIFAT32api(FSapi):
    def __init__(self, nifat_path: str) -> None:
        super().__init__()
        self.lib = ctypes.CDLL(nifat_path)
        ci_t = c_int
        cluster_offset_t = c_uint32
        buffer_t = POINTER(c_char)
        const_buffer_t = POINTER(c_char)

        class NIFAT32Params(ctypes.Structure):
            _fields_ = [
                ("fat_cache", c_char),
                ("bs_num", c_char),
                ("bs_count", c_char),
                ("ts", c_uint32),
                ("jc", c_ubyte),
            ]

        self.lib.NIFAT32_init.argtypes = [POINTER(NIFAT32Params)]
        self.lib.NIFAT32_init.restype = c_int

        self.lib.NIFAT32_repair_bootsectors.argtypes = []
        self.lib.NIFAT32_repair_bootsectors.restype = c_int

        self.lib.NIFAT32_unload.argtypes = []
        self.lib.NIFAT32_unload.restype = c_int

        self.lib.NIFAT32_content_exists.argtypes = [c_char_p]
        self.lib.NIFAT32_content_exists.restype = c_int

        class CInfo(Structure):
            _fields_ = [
                ("full_name", c_char * 12),
                ("file_name", c_char * 8),
                ("file_extension", c_char * 4),
                ("size", c_uint32),
                ("type", c_int),
            ]

        self.lib.NIFAT32_open_content.argtypes = [ci_t, c_char_p, ctypes.c_ubyte]
        self.lib.NIFAT32_open_content.restype = ci_t

        self.lib.NIFAT32_stat_content.argtypes = [ci_t, POINTER(CInfo)]
        self.lib.NIFAT32_stat_content.restype = c_int

        self.lib.NIFAT32_change_meta.argtypes = [ci_t, POINTER(CInfo)]
        self.lib.NIFAT32_change_meta.restype = c_int

        self.lib.NIFAT32_read_content2buffer.argtypes = [ci_t, cluster_offset_t, buffer_t, c_int]
        self.lib.NIFAT32_read_content2buffer.restype = c_int

        self.lib.NIFAT32_write_buffer2content.argtypes = [ci_t, cluster_offset_t, const_buffer_t, c_int]
        self.lib.NIFAT32_write_buffer2content.restype = c_int

        self.lib.NIFAT32_truncate_content.argtypes = [ci_t, cluster_offset_t, c_int]
        self.lib.NIFAT32_truncate_content.restype = c_int

        self.lib.NIFAT32_index_content.argtypes = [ci_t]
        self.lib.NIFAT32_index_content.restype = c_int

        self.lib.NIFAT32_close_content.argtypes = [ci_t]
        self.lib.NIFAT32_close_content.restype = c_int

        self.lib.NIFAT32_put_content.argtypes = [ci_t, POINTER(CInfo), c_int]
        self.lib.NIFAT32_put_content.restype = c_int

        self.lib.NIFAT32_copy_content.argtypes = [ci_t, ci_t, ctypes.c_char]
        self.lib.NIFAT32_copy_content.restype = c_int

        self.lib.NIFAT32_delete_content.argtypes = [ci_t]
        self.lib.NIFAT32_delete_content.restype = c_int

        self.lib.NIFAT32_repair_content.argtypes = [ci_t, c_int]
        self.lib.NIFAT32_repair_content.restype = c_int

    def open(self, **kwargs) -> File:
        return NIFAT32file(
            ci=self.lib.NIFAT32_open_content(kwargs.get("rci", -1), kwargs.get("path", None), kwargs.get("mode", 0)),
            fs=self
        )
