from framework.building.builder import ImageBuilder

class NIFAT32builder(ImageBuilder):
    def __init__(self, rpath: str, fpath: str) -> None:
        """NIFAT32 files builder.

        Args:
            rpath (str): Root path. Path to root folder of nifat32.
            fpath (str): Formatter path. Path to formatter tool.
        """
        super().__init__()
        self.tpath: str = rpath
        self.fpath: str = fpath
        
    def image_build(self) -> None:
        return super().image_build()
    
    def so_build(self) -> None:
        return super().so_build()
