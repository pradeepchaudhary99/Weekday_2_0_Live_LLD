class IFileSystemNode {
    getSize() {
        throw new Error("Not implemented");
    }
    rename(name) {
        throw new Error("Not implemented");
    }
    printDetails() {
        throw new Error("Not implemented");
    }
}

class File extends IFileSystemNode {
    constructor(name, size) {
        super();
        this.name = name;
        this.size = size;
        this.content = undefined;
    }

    getSize() {
        return this.size;
    }

    rename(name) {
        this.name = name;
    }

    printDetails() {
        console.log(`File: ${this.name}`);
    }
}

class Folder extends IFileSystemNode {
    constructor(name) {
        super();
        this.name = name;
        this.childs = new Map();
    }

    mkdir(name) {
        this.childs.set(name, new Folder(name));
    }

    touch(name, size) {
        this.childs.set(name, new File(name, size));
    }

    deleteFileSysteNode(name) {
        this.childs.delete(name);
    }

    getChild(name) {
        return this.childs.get(name);
    }

    getSize() {
        let size = 0;
        for (const node of this.childs.values()) {
            size += node.getSize();
        }
        return size;
    }

    rename(name) {
        console.log("renamed");
    }

    printDetails() {
        console.log(`Current Folder: ${this.name}`);
        for (const node of this.childs.values()) {
            node.printDetails();
        }
    }
}

class CompositionDesignPatternDemo {
    static main(args) {
        const root = new Folder("root");
        root.mkdir("movies");
    }
}

CompositionDesignPatternDemo.main([]);
