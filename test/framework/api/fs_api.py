# Filesystem API template
from abc import ABC, abstractmethod

class File(ABC):
    def __init__(self) -> None:
        super().__init__()

class FSapi(ABC):
    def __init__(self) -> None:
        super().__init__()
    
    @abstractmethod
    def open(self, **kwargs) -> File:
        """Filesystem open function.
        Use kwargs for passing necessary arguments. 

        Returns:
            File: file.
        """
        pass
    
    @abstractmethod
    def ropen(self, **kwargs) -> File:
        """Open root element of filesystem. If filesystem support several roots, use kwargs.

        Returns:
            File: Root element.
        """
        pass
    
    @abstractmethod
    def close(self, file: File) -> bool:
        """Filesystem close provided file.

        Args:
            file (File): File that should be closed.

        Returns:
            bool: True if success.
        """
        pass
    
    @abstractmethod
    def mkfile(self, **kwargs) -> bool:
        """Create file by path.

        Returns:
            bool: True if success.
        """
        pass
    
    @abstractmethod
    def mkrandfile(self, rdir: str | None, **kwargs) -> str:
        """Create random file somewhere in filesystem.

        Args:
            rdir (str | None): Root directory path.

        Returns:
            File: Return path to file.
        """
        pass
    
    @abstractmethod
    def mkdir(self, **kwargs) -> bool:
        """Create directory by path.

        Returns:
            bool: True if success.
        """
        pass
    
    @abstractmethod
    def mkranddir(self, rdir: str | None, **kwargs) -> str:
        """Create random directory somewhere if filesystem.

        Args:
            rdir (str | None): Root directory path.

        Returns:
            str: Return path to directory.
        """
        pass
    
    @abstractmethod
    def read(self, file: File, **kwargs) -> int:
        """Read data from file by offset to some location.

        Returns:
            int: Count of readden bytes.
        """
        pass
    
    @abstractmethod
    def write(self, file: File, **kwargs) -> int:
        """Write data to file by offset from some location.

        Returns:
            int: Count of written bytes.
        """
        pass
    
    @abstractmethod
    def trunc(self, file: File, offset: int, size: int) -> bool:
        """Truncate file (Change file size).

        Args:
            offset (int): Offset that will delete all start data from file.
            size (int): New file size in bytes.

        Returns:
            bool: True if truncate success.
        """
        pass
    
    @abstractmethod
    def rename(self, file: File, **kwargs) -> bool:
        """Rename file and change meta information.

        Returns:
            bool: True if rename success.
        """
        pass
    
    @abstractmethod
    def delete(self, file: File) -> bool:
        """Delete file.

        Returns:
            bool: True if delete success.
        """
        pass
    