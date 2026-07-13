#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

struct IFileSystemNode {
    virtual int getSize() = 0;
    virtual void rename(const std::string& name) = 0;
    virtual void printDetails() = 0;
    virtual ~IFileSystemNode() = default;
};

class File : public IFileSystemNode {
public:
    std::string name;
    int size;
    std::string content;

    File(std::string name, int size) : name(std::move(name)), size(size) {}

    int getSize() override {
        return size;
    }

    void rename(const std::string& newName) override {
        name = newName;
    }

    void printDetails() override {
        std::cout << "File: " << name << std::endl;
    }
};

class Folder : public IFileSystemNode {
public:
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<IFileSystemNode>> childs;

    explicit Folder(std::string name) : name(std::move(name)) {}

    void mkdir(const std::string& dirName) {
        childs[dirName] = std::make_shared<Folder>(dirName);
    }

    void touch(const std::string& fileName, int size) {
        childs[fileName] = std::make_shared<File>(fileName, size);
    }

    void deleteFileSystemNode(const std::string& nodeName) {
        childs.erase(nodeName);
    }

    std::shared_ptr<IFileSystemNode> getChild(const std::string& childName) {
        auto it = childs.find(childName);
        return it != childs.end() ? it->second : nullptr;
    }

    int getSize() override {
        int total = 0;
        for (auto& [childName, node] : childs) {
            total += node->getSize();
        }
        return total;
    }

    void rename(const std::string& newName) override {
        std::cout << "renamed" << std::endl;
    }

    void printDetails() override {
        std::cout << "Current Folder: " << name << std::endl;
        for (auto& [childName, node] : childs) {
            node->printDetails();
        }
    }
};

int main() {
    auto root = std::make_shared<Folder>("root");
    root->mkdir("movies");

    return 0;
}
