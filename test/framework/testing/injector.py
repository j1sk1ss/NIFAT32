# Bit-flips injector module
from abc import ABC, abstractmethod
from typing import BinaryIO
import mmap
import random

class Strategy(ABC):
    """Injector strategy base class
    """
    def __init__(self) -> None:
        pass
    
    @abstractmethod
    def inject(self, mm: mmap.mmap) -> None:
        pass

class RandomBitflipStrategy(Strategy):
    def __init__(self, num_flips: int = 10, seed: int = 123456789) -> None:
        self.num_flips = num_flips
        self._rng = random.Random(seed)

    def inject(self, mm: mmap.mmap) -> None:
        size = len(mm)
        for _ in range(self.num_flips):
            idx = self._rng.randint(0, size - 1)
            bit = self._rng.randint(0, 7)
            mm[idx] ^= (1 << bit)

class NormalDistributionStrategy(Strategy):
    def __init__(
        self,
        num_flips: int = 10,
        mean_ratio: float = 0.5,
        stddev_ratio: float = 0.15,
        seed: int = 123456789
    ) -> None:
        """
        num_flips: Bitflip count
        mean_ratio: average position (0.0 - 1.0) in image
        stddev_ratio: stddev
        seed: seed
        """
        self.num_flips = num_flips
        self.mean_ratio = mean_ratio
        self.stddev_ratio = stddev_ratio
        self._rng = random.Random(seed)

    def inject(self, mm: mmap.mmap) -> None:
        size = len(mm)
        mean = self.mean_ratio * size
        stddev = self.stddev_ratio * size

        for _ in range(self.num_flips):
            idx = int(self._rng.gauss(mu=mean, sigma=stddev))
            idx = max(0, min(size - 1, idx))

            bit = self._rng.randint(0, 7)
            mm[idx] ^= (1 << bit)

class ScratchStrategy(Strategy):
    def __init__(self, scratch_length: int = 1024, width: int = 1, intensity: float = 0.7):
        self.scratch_length = scratch_length
        self.width = width
        self.intensity = intensity

    def inject(self, mm: mmap.mmap) -> None:
        size = len(mm)
        if size == 0:
            return

        start_pos = random.randint(0, size - self.scratch_length)
        start_pos -= start_pos % self.width

        for offset in range(start_pos, start_pos + self.scratch_length):
            if random.random() > self.intensity:
                continue

            for track in range(self.width):
                pos = offset + track
                if pos < size:
                    flip_mask = random.randint(1, 255)
                    mm[pos] ^= flip_mask

class Injector:
    """Injector base class
    """
    def __init__(self, image: str, strat: Strategy) -> None:
        self.image: str = image
        self.strat: Strategy = strat
    
    def inject(self) -> None:
        with open(self.image, "r+b") as f:
            with mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_WRITE) as mm:
                self.strat.inject(mm)
                mm.flush()
                