# Building image module
from abc import ABC, abstractmethod

class ImageBuilder(ABC):
    def __init__(self) -> None:
        super().__init__()

    @abstractmethod
    def image_build(self) -> None:
        pass

    @abstractmethod
    def so_build(self) -> None:
        pass

    @abstractmethod
    def clean(self) -> None:
        pass
