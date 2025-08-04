# Bit-flips injector module

class Strategy:
    """Injector strategy base class
    """
    def __init__(self) -> None:
        pass

class Injector:
    """Injector base class
    """
    def __init__(self, image: str, strat: Strategy | None = None) -> None:
        pass
    
    def inject(self) -> None:
        pass