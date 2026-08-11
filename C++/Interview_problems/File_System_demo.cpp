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

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

enum class UserType { USER = 0, ADMIN = 1 };

std::string userTypeName(UserType type) { return type == UserType::ADMIN ? "ADMIN" : "USER"; }

struct User {
    std::string name;
    UserType role;
};

class PathNotFoundError : public std::runtime_error {
public:
    explicit PathNotFoundError(const std::string& message) : std::runtime_error(message) {}
};

class PathAlreadyExistsError : public std::runtime_error {
public:
    explicit PathAlreadyExistsError(const std::string& message) : std::runtime_error(message) {}
};

class PermissionDeniedError : public std::runtime_error {
public:
    explicit PermissionDeniedError(const std::string& message) : std::runtime_error(message) {}
};

class Directory;

class FileSystemNode {
public:
    std::string name;
    Directory* parent;

    FileSystemNode(std::string name, Directory* parent) : name(std::move(name)), parent(parent) {}
    virtual ~FileSystemNode() = default;

    virtual long getSize() const = 0;
    virtual bool isDirectory() const = 0;
};

class File : public FileSystemNode {
public:
    std::string content;

    File(std::string name, Directory* parent) : FileSystemNode(std::move(name), parent) {}

    std::string read() const { return content; }
    void write(const std::string& newContent) { content = newContent; }
    void append(const std::string& more) { content += more; }

    long getSize() const override { return static_cast<long>(content.size()); }
    bool isDirectory() const override { return false; }
};

class Directory : public FileSystemNode {
public:
    std::map<std::string, std::shared_ptr<FileSystemNode>> children;
    UserType requiredRole;

    Directory(std::string name, Directory* parent, UserType requiredRole = UserType::USER)
        : FileSystemNode(std::move(name), parent), requiredRole(requiredRole) {}

    void addChild(std::shared_ptr<FileSystemNode> child) { children[child->name] = std::move(child); }

    std::shared_ptr<FileSystemNode> getChild(const std::string& name) const {
        auto it = children.find(name);
        return it != children.end() ? it->second : nullptr;
    }

    void removeChild(const std::string& name) { children.erase(name); }

    long getSize() const override {
        long total = 0;
        for (const auto& [_, child] : children) {
            total += child->getSize();
        }
        return total;
    }

    bool isDirectory() const override { return true; }
};

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) parts.push_back(part);
    }
    if (parts.empty()) {
        throw std::invalid_argument("Path must not be empty");
    }
    return parts;
}

class FileSystemManager {
public:
    std::shared_ptr<Directory> root = std::make_shared<Directory>("/", nullptr);

    Directory* resolveParent(const std::vector<std::string>& parts) const {
        Directory* current = root.get();
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            auto child = current->getChild(parts[i]);
            if (!child || !child->isDirectory()) {
                throw PathNotFoundError("No such directory: " + parts[i]);
            }
            current = static_cast<Directory*>(child.get());
        }
        return current;
    }

    std::shared_ptr<FileSystemNode> resolve(const std::string& path) const {
        auto parts = splitPath(path);
        std::shared_ptr<FileSystemNode> current = root;
        for (const auto& part : parts) {
            if (!current->isDirectory()) {
                throw PathNotFoundError("Not a directory: " + current->name);
            }
            auto dir = static_cast<Directory*>(current.get());
            auto child = dir->getChild(part);
            if (!child) {
                throw PathNotFoundError("No such path: " + path);
            }
            current = child;
        }
        return current;
    }

    void checkPermission(Directory* directory, const User& user) const {
        if (user.role < directory->requiredRole) {
            throw PermissionDeniedError("User " + user.name + " (" + userTypeName(user.role) +
                                         ") lacks permission for " + directory->name +
                                         " (requires " + userTypeName(directory->requiredRole) + ")");
        }
    }

    std::shared_ptr<Directory> createDirectory(const User& user, const std::string& path,
                                                UserType requiredRole = UserType::USER) {
        auto parts = splitPath(path);
        Directory* parent = resolveParent(parts);
        checkPermission(parent, user);
        const std::string& name = parts.back();
        if (parent->getChild(name)) {
            throw PathAlreadyExistsError("Path already exists: " + path);
        }
        auto directory = std::make_shared<Directory>(name, parent, requiredRole);
        parent->addChild(directory);
        return directory;
    }

    std::shared_ptr<File> createFile(const User& user, const std::string& path) {
        auto parts = splitPath(path);
        Directory* parent = resolveParent(parts);
        checkPermission(parent, user);
        const std::string& name = parts.back();
        if (parent->getChild(name)) {
            throw PathAlreadyExistsError("Path already exists: " + path);
        }
        auto file = std::make_shared<File>(name, parent);
        parent->addChild(file);
        return file;
    }

    void writeFile(const User& user, const std::string& path, const std::string& content) {
        auto node = resolve(path);
        auto file = std::dynamic_pointer_cast<File>(node);
        if (!file) {
            throw std::invalid_argument("Not a file: " + path);
        }
        checkPermission(file->parent, user);
        file->write(content);
    }

    std::string readFile(const User& user, const std::string& path) const {
        (void)user;
        auto node = resolve(path);
        auto file = std::dynamic_pointer_cast<File>(node);
        if (!file) {
            throw std::invalid_argument("Not a file: " + path);
        }
        return file->read();
    }

    void deletePath(const User& user, const std::string& path) {
        auto node = resolve(path);
        Directory* parent = node->parent;
        if (!parent) {
            throw std::invalid_argument("Cannot delete the root directory");
        }
        checkPermission(parent, user);
        parent->removeChild(node->name);
    }

    long getSize(const std::string& path) const { return resolve(path)->getSize(); }

    void move(const User& user, const std::string& source, const std::string& destination) {
        auto node = resolve(source);
        Directory* oldParent = node->parent;
        if (!oldParent) {
            throw std::invalid_argument("Cannot move the root directory");
        }
        checkPermission(oldParent, user);

        auto destParts = splitPath(destination);
        Directory* newParent = resolveParent(destParts);
        checkPermission(newParent, user);
        const std::string& newName = destParts.back();
        if (newParent->getChild(newName)) {
            throw PathAlreadyExistsError("Path already exists: " + destination);
        }

        oldParent->removeChild(node->name);
        node->name = newName;
        node->parent = newParent;
        newParent->addChild(node);
    }
};

int main() {
    FileSystemManager fs;
    User admin{"root-admin", UserType::ADMIN};
    User guest{"guest", UserType::USER};

    fs.createDirectory(admin, "/docs");
    fs.createFile(admin, "/docs/notes.txt");
    fs.writeFile(admin, "/docs/notes.txt", "Meeting notes: LLD review at 3pm");

    fs.createDirectory(admin, "/docs/drafts");
    fs.createFile(admin, "/docs/drafts/todo.txt");
    fs.writeFile(admin, "/docs/drafts/todo.txt", "1. Finish parser\n2. Write tests");

    std::cout << "/docs size: " << fs.getSize("/docs") << " bytes\n";

    std::cout << "\nCreating an admin-only directory and writing to it as a guest:\n";
    fs.createDirectory(admin, "/secure", UserType::ADMIN);
    fs.createFile(admin, "/secure/keys.txt");
    try {
        fs.writeFile(guest, "/secure/keys.txt", "should not be allowed");
    } catch (const PermissionDeniedError& e) {
        std::cout << "  " << e.what() << "\n";
    }

    std::cout << "\nWriting to it as an admin succeeds:\n";
    fs.writeFile(admin, "/secure/keys.txt", "api-key-12345");
    std::cout << "  /secure/keys.txt -> '" << fs.readFile(admin, "/secure/keys.txt") << "'\n";

    std::cout << "\nMoving /docs/drafts/todo.txt to /docs/todo.txt:\n";
    fs.move(admin, "/docs/drafts/todo.txt", "/docs/todo.txt");
    std::cout << "  /docs/todo.txt -> '" << fs.readFile(admin, "/docs/todo.txt") << "'\n";

    std::cout << "\nDeleting /docs/drafts (now empty):\n";
    fs.deletePath(admin, "/docs/drafts");
    std::cout << "  /docs size after cleanup: " << fs.getSize("/docs") << " bytes\n";

    return 0;
}
