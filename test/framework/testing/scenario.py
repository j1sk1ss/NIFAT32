from framework.api.fs_api import FSapi
from framework.testing.injector import Injector
from abc import ABC, abstractmethod

class Action(ABC):
    def __init__(self) -> None:
        super().__init__()

    @abstractmethod
    def action(self) -> None:
        pass

class Injection(Action):
    def __init__(self, injector: Injector) -> None:
        super().__init__()
        self.injector: Injector = injector

    def action(self) -> None:
        self.injector.inject()

class MKRandomFile(Action):
    def __init__(self, fs: FSapi) -> None:
        super().__init__()
        self.fs: FSapi = fs

    def action(self) -> None:
        self.fs.mkfile(**self.args)

class MKdir(Action):
    def __init__(self, fs: FSapi, **kwargs) -> None:
        super().__init__()
        self.fs: FSapi = fs
        self.args = kwargs

    def action(self) -> None:
        self.fs.mkdir(**self.args)

class Scenario:
    def __init__(self) -> None:
        pass
