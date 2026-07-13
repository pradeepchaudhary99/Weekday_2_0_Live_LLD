from abc import ABC, abstractmethod
from typing import Dict, Optional


class IFileSystemNode(ABC):
    @abstractmethod
    def get_size(self) -> int:
        pass

    @abstractmethod
    def rename(self, name: str) -> None:
        pass

    @abstractmethod
    def print_details(self) -> None:
        pass


class File(IFileSystemNode):
    def __init__(self, name: str, size: int):
        self.name = name
        self.size = size
        self.content: str = ""

    def get_size(self) -> int:
        return self.size

    def rename(self, name: str) -> None:
        self.name = name

    def print_details(self) -> None:
        print(f"File: {self.name}")


class Folder(IFileSystemNode):
    def __init__(self, name: str):
        self.name = name
        self.childs: Dict[str, IFileSystemNode] = {}

    def mkdir(self, name: str) -> None:
        self.childs[name] = Folder(name)

    def touch(self, name: str, size: int) -> None:
        self.childs[name] = File(name, size)

    def delete_file_system_node(self, name: str) -> None:
        self.childs.pop(name, None)

    def get_child(self, name: str) -> Optional[IFileSystemNode]:
        return self.childs.get(name)

    def get_size(self) -> int:
        return sum(node.get_size() for node in self.childs.values())

    def rename(self, name: str) -> None:
        print("renamed")

    def print_details(self) -> None:
        print(f"Current Folder: {self.name}")
        for node in self.childs.values():
            node.print_details()


if __name__ == "__main__":
    root = Folder("root")
    root.mkdir("movies")
