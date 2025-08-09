import random
from framework.api.fs_api import FSapi, File
from abc import ABC, abstractmethod

class Action(ABC):
    def __init__(self) -> None:
        super().__init__()

    @abstractmethod
    def action(self, api: FSapi) -> None:
        pass

class FillFileSystem(Action):
    """Create a bunch of files and directories.

    Args:
        Action (_type_): Base Action class
    """
    def __init__(
        self, files: int = -1, dirs: int = -1, summary: int = -1,
        min_depth: int = -1, max_depth: int = -1, seed: int = 1234
    ) -> None:
        """Init scenario

        Args:
            files (int): Count of files in fs. 
            dirs (int): Count of directories in fs.
            summary (int): Summary count of files and directories.
            min_depth (int): Directory minimum depth. Example with min. depth == 1:
                             All files and directories will store at root directories XXXX/file, YXYX/dir...
            max_depth (int): Directory maximum depth. Example with max. depth == 2:
                             All files and directories will be prohibited from being stored in two directories.
        """
        super().__init__()
        self.files: int = files
        self.dirs: int = dirs
        self.summary: int = summary
        self.min_depth: int = min_depth
        self.max_depth: int = max_depth
        self.rng = random.Random(x=seed)
        
    def _gen_depth(self) -> int:
        if self.min_depth == -1 and self.max_depth == -1:
            return self.rng.randint(1, 5)
        elif self.min_depth != -1 and self.max_depth != -1:
            return self.rng.randint(self.min_depth, self.max_depth)
        elif self.min_depth != -1:
            return self.rng.randint(self.min_depth, self.min_depth + 2)
        
        return self.rng.randint(1, self.max_depth)
        
    def action(self, api: FSapi) -> None:
        files = self.files
        dirs = self.dirs

        if self.summary != -1 and (files == -1 or dirs == -1):
            dirs = self.rng.randint(0, self.summary)
            files = self.summary - dirs

        if files <= 0 and dirs <= 0:
            return

        created_dirs: list[str] = []
        for _ in range(dirs):
            depth = self._gen_depth()
            current_root = None
            for _ in range(depth):
                new_dir = api.mkranddir(rdir=current_root)
                current_root = f"{current_root}/{new_dir}"
                created_dirs.append(current_root)

        if not created_dirs:
            created_dirs.append(None)

        for _ in range(files):
            parent = self.rng.choice(created_dirs)
            api.mkrandfile(rdir=parent)

class ClearFileSystem(Action):
    """Clear file system by recursive delete

    Args:
        Action (_type_): Base Action class
    """
    def __init__(self) -> None:
        super().__init__()
        
    def action(self, api: FSapi) -> None:
        rfl: File = api.ropen()
        api.delete(file=rfl)
        api.close(rfl)

class Scenario:
    def __init__(self, actions: list[Action]) -> None:
        self.actions: list[Action] = actions
        
    def start(self, fs: FSapi) -> None:
        for i in self.actions:
            i.action(fs)
        
