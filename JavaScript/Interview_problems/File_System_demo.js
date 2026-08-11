/*
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
    and an optional requiredRole: an operation that mutates something
    under this directory (or the directory itself) must be performed by a
    user whose role is at least requiredRole.

    FileSystemManager resolves "/"-separated paths by walking the tree
    from the root and performs permission checks against the *parent*
    directory of the node being mutated (create/write/delete/move all
    change the parent's contents).

Core Entities:
    UserType / User
    FileSystemNode (abstract) / File / Directory
    FileSystemManager
================================================================================
*/

const UserType = Object.freeze({ USER: 0, ADMIN: 1 });

function userTypeName(role) {
    return role === UserType.ADMIN ? "ADMIN" : "USER";
}

class User {
    constructor(name, role) {
        this.name = name;
        this.role = role;
    }
}

class PathNotFoundError extends Error {}
class PathAlreadyExistsError extends Error {}
class PermissionDeniedError extends Error {}

class FileSystemNode {
    constructor(name, parent) {
        this.name = name;
        this.parent = parent;
    }

    getSize() {
        throw new Error("getSize must be implemented by subclasses");
    }

    isDirectory() {
        throw new Error("isDirectory must be implemented by subclasses");
    }
}

class FSFile extends FileSystemNode {
    constructor(name, parent) {
        super(name, parent);
        this.content = "";
    }

    read() {
        return this.content;
    }

    write(content) {
        this.content = content;
    }

    append(content) {
        this.content += content;
    }

    getSize() {
        return this.content.length;
    }

    isDirectory() {
        return false;
    }
}

class Directory extends FileSystemNode {
    constructor(name, parent, requiredRole = UserType.USER) {
        super(name, parent);
        this.children = new Map();
        this.requiredRole = requiredRole;
    }

    addChild(child) {
        this.children.set(child.name, child);
    }

    getChild(name) {
        return this.children.get(name) ?? null;
    }

    removeChild(name) {
        this.children.delete(name);
    }

    getSize() {
        let total = 0;
        for (const child of this.children.values()) {
            total += child.getSize();
        }
        return total;
    }

    isDirectory() {
        return true;
    }
}

function splitPath(path) {
    const parts = path.split("/").filter((part) => part.length > 0);
    if (parts.length === 0) {
        throw new Error("Path must not be empty");
    }
    return parts;
}

class FileSystemManager {
    constructor() {
        this.root = new Directory("/", null);
    }

    _resolveParent(parts) {
        let current = this.root;
        for (let i = 0; i < parts.length - 1; i++) {
            const child = current.getChild(parts[i]);
            if (!child || !child.isDirectory()) {
                throw new PathNotFoundError(`No such directory: ${parts[i]}`);
            }
            current = child;
        }
        return current;
    }

    _resolve(path) {
        const parts = splitPath(path);
        let current = this.root;
        for (const part of parts) {
            if (!current.isDirectory()) {
                throw new PathNotFoundError(`Not a directory: ${current.name}`);
            }
            const child = current.getChild(part);
            if (!child) {
                throw new PathNotFoundError(`No such path: ${path}`);
            }
            current = child;
        }
        return current;
    }

    _checkPermission(directory, user) {
        if (user.role < directory.requiredRole) {
            throw new PermissionDeniedError(
                `User ${user.name} (${userTypeName(user.role)}) lacks permission for ${directory.name} ` +
                `(requires ${userTypeName(directory.requiredRole)})`);
        }
    }

    createDirectory(user, path, requiredRole = UserType.USER) {
        const parts = splitPath(path);
        const parent = this._resolveParent(parts);
        this._checkPermission(parent, user);
        const name = parts[parts.length - 1];
        if (parent.getChild(name)) {
            throw new PathAlreadyExistsError(`Path already exists: ${path}`);
        }
        const directory = new Directory(name, parent, requiredRole);
        parent.addChild(directory);
        return directory;
    }

    createFile(user, path) {
        const parts = splitPath(path);
        const parent = this._resolveParent(parts);
        this._checkPermission(parent, user);
        const name = parts[parts.length - 1];
        if (parent.getChild(name)) {
            throw new PathAlreadyExistsError(`Path already exists: ${path}`);
        }
        const file = new FSFile(name, parent);
        parent.addChild(file);
        return file;
    }

    writeFile(user, path, content) {
        const node = this._resolve(path);
        if (!(node instanceof FSFile)) {
            throw new Error(`Not a file: ${path}`);
        }
        this._checkPermission(node.parent, user);
        node.write(content);
    }

    readFile(_user, path) {
        const node = this._resolve(path);
        if (!(node instanceof FSFile)) {
            throw new Error(`Not a file: ${path}`);
        }
        return node.read();
    }

    delete(user, path) {
        const node = this._resolve(path);
        const parent = node.parent;
        if (!parent) {
            throw new Error("Cannot delete the root directory");
        }
        this._checkPermission(parent, user);
        parent.removeChild(node.name);
    }

    getSize(path) {
        return this._resolve(path).getSize();
    }

    move(user, source, destination) {
        const node = this._resolve(source);
        const oldParent = node.parent;
        if (!oldParent) {
            throw new Error("Cannot move the root directory");
        }
        this._checkPermission(oldParent, user);

        const destParts = splitPath(destination);
        const newParent = this._resolveParent(destParts);
        this._checkPermission(newParent, user);
        const newName = destParts[destParts.length - 1];
        if (newParent.getChild(newName)) {
            throw new PathAlreadyExistsError(`Path already exists: ${destination}`);
        }

        oldParent.removeChild(node.name);
        node.name = newName;
        node.parent = newParent;
        newParent.addChild(node);
    }
}

function main() {
    const fs = new FileSystemManager();
    const admin = new User("root-admin", UserType.ADMIN);
    const guest = new User("guest", UserType.USER);

    fs.createDirectory(admin, "/docs");
    fs.createFile(admin, "/docs/notes.txt");
    fs.writeFile(admin, "/docs/notes.txt", "Meeting notes: LLD review at 3pm");

    fs.createDirectory(admin, "/docs/drafts");
    fs.createFile(admin, "/docs/drafts/todo.txt");
    fs.writeFile(admin, "/docs/drafts/todo.txt", "1. Finish parser\n2. Write tests");

    console.log(`/docs size: ${fs.getSize("/docs")} bytes`);

    console.log("\nCreating an admin-only directory and writing to it as a guest:");
    fs.createDirectory(admin, "/secure", UserType.ADMIN);
    fs.createFile(admin, "/secure/keys.txt");
    try {
        fs.writeFile(guest, "/secure/keys.txt", "should not be allowed");
    } catch (e) {
        if (e instanceof PermissionDeniedError) {
            console.log(`  ${e.message}`);
        } else {
            throw e;
        }
    }

    console.log("\nWriting to it as an admin succeeds:");
    fs.writeFile(admin, "/secure/keys.txt", "api-key-12345");
    console.log(`  /secure/keys.txt -> '${fs.readFile(admin, "/secure/keys.txt")}'`);

    console.log("\nMoving /docs/drafts/todo.txt to /docs/todo.txt:");
    fs.move(admin, "/docs/drafts/todo.txt", "/docs/todo.txt");
    console.log(`  /docs/todo.txt -> '${fs.readFile(admin, "/docs/todo.txt")}'`);

    console.log("\nDeleting /docs/drafts (now empty):");
    fs.delete(admin, "/docs/drafts");
    console.log(`  /docs size after cleanup: ${fs.getSize("/docs")} bytes`);
}

main();
