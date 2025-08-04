# FS test template
from enum import Enum
from framework.api.fs_api import FSapi
from abc import ABC, abstractmethod

class ScenarioAction(Enum):
    INJECT = 1
    TEST = 2

class Scenario:
    def __init__(self, scenario: list[tuple[ScenarioAction, int]]) -> None:
        self.body: list[tuple[ScenarioAction, int]] = scenario

class Test(ABC):
    def __init__(self) -> None:
        super().__init__()

    @abstractmethod
    def work(self, fs: FSapi) -> int:
        pass
    
    def invoke(self, fs: FSapi) -> int:
        return self.work(fs=fs)

class Tester:
    def __init__(
        self, tests: list[Test], scenario: Scenario, fs: FSapi
    ) -> None:
        self.body: list[Test] = tests
        self.scenario: Scenario = scenario
        self.fs: FSapi = fs

    def start(self) -> list[int]:
        result: list[int] = []
        for test in self.body:
            result.append(test.invoke(fs=self.fs))

        return result
