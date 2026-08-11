"""
================================================================================
LLD: In-Memory File System
================================================================================

Functional Requirements:
    1. Create a directory at a given path.
    2. Create a file at a given path.
    3. Write (overwrite) content to a file.
    4. Read the content of a file.
    5. Delete a file or a directory (recursively).
    6. Calculate the size of a file or a directory (recursive sum).
    7. Move a file or directory from one path to another.
    8. Enforce permissions at the directory level for ADMIN vs USER.

Non-Functional Requirements:
    1. Extensibility: adding a new node type (e.g. symlink) shouldn't
       require touching path-resolution logic.
    2. Predictable errors: an operation on a path that doesn't exist, or
       one a user lacks permission for, fails with a clear exception
       instead of corrupting the tree.

Design:
    FileSystemNode (abstract) is the common base for File and Directory --
    both know their name, parent, and how to report their own size.
    Directory additionally holds a name -> FileSystemNode map of children
    and an optional `required_role`: an operation that mutates something
    under this directory (or the directory itself) must be performed by a
    user whose role is at least `required_role`.

    FileSystemManager resolves "/"-separated paths by walking the tree
    from the root and performs permission checks against the *parent*
    directory of the node being mutated (create/write/delete/move all
    change the parent's contents).

Core Entities:
    UserType / User
    FileSystemNode (abstract) / File / Directory
    FileSystemManager
================================================================================
"""

from abc import ABC, abstractmethod
from enum import IntEnum
from typing import Dict, Optional


class UserType(IntEnum):
    USER = 0
    ADMIN = 1


class User:
    def __init__(self, name: str, role: UserType):
        self.name = name
        self.role = role


class PathNotFoundError(RuntimeError):
    pass


class PathAlreadyExistsError(RuntimeError):
    pass


class PermissionDeniedError(RuntimeError):
    pass


class FileSystemNode(ABC):
    def __init__(self, name: str, parent: Optional["Directory"]):
        self.name = name
        self.parent = parent

    @abstractmethod
    def get_size(self) -> int:
        raise NotImplementedError

    @abstractmethod
    def is_directory(self) -> bool:
        raise NotImplementedError


class File(FileSystemNode):
    def __init__(self, name: str, parent: Optional["Directory"]):
        super().__init__(name, parent)
        self.content = ""

    def read(self) -> str:
        return self.content

    def write(self, content: str) -> None:
        self.content = content

    def append(self, content: str) -> None:
        self.content += content

    def get_size(self) -> int:
        return len(self.content)

    def is_directory(self) -> bool:
        return False


class Directory(FileSystemNode):
    def __init__(self, name: str, parent: Optional["Directory"], required_role: UserType = UserType.USER):
        super().__init__(name, parent)
        self.children: Dict[str, FileSystemNode] = {}
        self.required_role = required_role

    def add_child(self, child: FileSystemNode) -> None:
        self.children[child.name] = child

    def get_child(self, name: str) -> Optional[FileSystemNode]:
        return self.children.get(name)

    def remove_child(self, name: str) -> Optional[FileSystemNode]:
        return self.children.pop(name, None)

    def get_size(self) -> int:
        return sum(child.get_size() for child in self.children.values())

    def is_directory(self) -> bool:
        return True


def _split_path(path: str):
    parts = [part for part in path.split("/") if part]
    if not parts:
        raise ValueError("Path must not be empty")
    return parts


class FileSystemManager:
    def __init__(self):
        self.root = Directory("/", None)

    def _resolve_parent(self, parts) -> Directory:
        current = self.root
        for part in parts[:-1]:
            child = current.get_child(part)
            if child is None or not child.is_directory():
                raise PathNotFoundError(f"No such directory: {part}")
            current = child
        return current

    def _resolve(self, path: str) -> FileSystemNode:
        parts = _split_path(path)
        current: FileSystemNode = self.root
        for part in parts:
            if not current.is_directory():
                raise PathNotFoundError(f"Not a directory: {current.name}")
            child = current.get_child(part)
            if child is None:
                raise PathNotFoundError(f"No such path: {path}")
            current = child
        return current

    def _check_permission(self, directory: Directory, user: User) -> None:
        if user.role < directory.required_role:
            raise PermissionDeniedError(
                f"User {user.name} ({user.role.name}) lacks permission for {directory.name} "
                f"(requires {directory.required_role.name})")

    def create_directory(self, user: User, path: str, required_role: UserType = UserType.USER) -> Directory:
        parts = _split_path(path)
        parent = self._resolve_parent(parts)
        self._check_permission(parent, user)
        name = parts[-1]
        if parent.get_child(name) is not None:
            raise PathAlreadyExistsError(f"Path already exists: {path}")
        directory = Directory(name, parent, required_role)
        parent.add_child(directory)
        return directory

    def create_file(self, user: User, path: str) -> File:
        parts = _split_path(path)
        parent = self._resolve_parent(parts)
        self._check_permission(parent, user)
        name = parts[-1]
        if parent.get_child(name) is not None:
            raise PathAlreadyExistsError(f"Path already exists: {path}")
        file = File(name, parent)
        parent.add_child(file)
        return file

    def write_file(self, user: User, path: str, content: str) -> None:
        node = self._resolve(path)
        if not isinstance(node, File):
            raise ValueError(f"Not a file: {path}")
        self._check_permission(node.parent, user)
        node.write(content)

    def read_file(self, user: User, path: str) -> str:
        node = self._resolve(path)
        if not isinstance(node, File):
            raise ValueError(f"Not a file: {path}")
        return node.read()

    def delete(self, user: User, path: str) -> None:
        node = self._resolve(path)
        parent = node.parent
        if parent is None:
            raise ValueError("Cannot delete the root directory")
        self._check_permission(parent, user)
        parent.remove_child(node.name)

    def get_size(self, path: str) -> int:
        return self._resolve(path).get_size()

    def move(self, user: User, source: str, destination: str) -> None:
        node = self._resolve(source)
        old_parent = node.parent
        if old_parent is None:
            raise ValueError("Cannot move the root directory")
        self._check_permission(old_parent, user)

        dest_parts = _split_path(destination)
        new_parent = self._resolve_parent(dest_parts)
        self._check_permission(new_parent, user)
        new_name = dest_parts[-1]
        if new_parent.get_child(new_name) is not None:
            raise PathAlreadyExistsError(f"Path already exists: {destination}")

        old_parent.remove_child(node.name)
        node.name = new_name
        node.parent = new_parent
        new_parent.add_child(node)


def main() -> None:
    fs = FileSystemManager()
    admin = User("root-admin", UserType.ADMIN)
    guest = User("guest", UserType.USER)

    fs.create_directory(admin, "/docs")
    fs.create_file(admin, "/docs/notes.txt")
    fs.write_file(admin, "/docs/notes.txt", "Meeting notes: LLD review at 3pm")

    fs.create_directory(admin, "/docs/drafts")
    fs.create_file(admin, "/docs/drafts/todo.txt")
    fs.write_file(admin, "/docs/drafts/todo.txt", "1. Finish parser\n2. Write tests")

    print(f"/docs size: {fs.get_size('/docs')} bytes")

    print("\nCreating an admin-only directory and writing to it as a guest:")
    fs.create_directory(admin, "/secure", required_role=UserType.ADMIN)
    fs.create_file(admin, "/secure/keys.txt")
    try:
        fs.write_file(guest, "/secure/keys.txt", "should not be allowed")
    except PermissionDeniedError as e:
        print(f"  {e}")

    print("\nWriting to it as an admin succeeds:")
    fs.write_file(admin, "/secure/keys.txt", "api-key-12345")
    print(f"  /secure/keys.txt -> '{fs.read_file(admin, '/secure/keys.txt')}'")

    print("\nMoving /docs/drafts/todo.txt to /docs/todo.txt:")
    fs.move(admin, "/docs/drafts/todo.txt", "/docs/todo.txt")
    print(f"  /docs/todo.txt -> '{fs.read_file(admin, '/docs/todo.txt')}'")

    print("\nDeleting /docs/drafts (now empty):")
    fs.delete(admin, "/docs/drafts")
    print(f"  /docs size after cleanup: {fs.get_size('/docs')} bytes")


if __name__ == "__main__":
    main()
