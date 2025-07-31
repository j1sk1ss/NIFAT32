# Filesystem API template
from abc import ABC, abstractmethod

class File(ABC):
    def __init__(self) -> None:
        super().__init__()
        
    @abstractmethod
    def read(self, **kwargs) -> int:
        pass
    
    @abstractmethod
    def write(self, **kwargs) -> int:
        pass
    
    @abstractmethod
    def trunc(self, size: int) -> bool:
        pass
    
    @abstractmethod
    def rename(self, **kwargs) -> bool:
        pass
    
    @abstractmethod
    def delete(self) -> bool:
        pass

class FSapi(ABC):
    def __init__(self) -> None:
        super().__init__()
    
    @abstractmethod
    def open(self, path: str, **kwargs) -> File:
        """open function

        Args:
            path (str): path to file

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