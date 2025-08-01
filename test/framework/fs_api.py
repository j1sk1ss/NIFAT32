# Filesystem API template
from abc import ABC, abstractmethod

class File(ABC):
    def __init__(self) -> None:
        super().__init__()
        
    @abstractmethod
    def read(self, **kwargs) -> int:
        """Read data from file by offset

        Returns:
            int: Count of readden bytes
        """
        pass
    
    @abstractmethod
    def write(self, **kwargs) -> int:
        """Write data to file by offset

        Returns:
            int: Count of written bytes
        """
        pass
    
    @abstractmethod
    def trunc(self, size: int) -> bool:
        """Truncate file (Change file size)

        Args:
            size (int): New file size in bytes

        Returns:
            bool: True if truncate success
        """
        pass
    
    @abstractmethod
    def rename(self, **kwargs) -> bool:
        """Rename file and change meta information

        Returns:
            bool: True if rename success
        """
        pass
    
    @abstractmethod
    def delete(self) -> bool:
        """Delete file

        Returns:
            bool: True if delete success
        """
        pass

class FSapi(ABC):
    def __init__(self) -> None:
        super().__init__()
    
    @abstractmethod
    def open(self, **kwargs) -> File:
        """open function

        Returns:
            File: file
        """
        pass
    
    @abstractmethod
    def close(self, file: File) -> bool:
        """close provided file

        Args:
            file (File): File that should be closed

        Returns:
            bool: True if success
        """
        pass
    
    @abstractmethod
    def mkfile(self, **kwargs) -> bool:
        """Create file

        Returns:
            bool: True if success
        """
        pass
    
    @abstractmethod
    def mkdir(self, **kwargs) -> bool:
        """Create directory

        Returns:
            bool: True if success
        """
        pass