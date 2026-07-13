import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

interface IFileSystemNode{
    int getSize();
    void rename(String name);
    void printDetails();
}

class File implements IFileSystemNode{
    String name;
    int size;
    String content;
    public File(String name, int size){
        this.name = name;
        this.size = size;
    }
    @Override
    public int getSize() {
        return size;
    }

    @Override
    public void rename(String name) {
        this.name = name;
    }

    @Override
    public void printDetails() {
        System.out.println("File: "+name);
    }

}

class Folder implements IFileSystemNode{
    String name;
    Map<String, IFileSystemNode> childs;

    public Folder(String name){
        this.name = name;
        childs = new HashMap<>();
    }
    
    void mkdir(String name){
        childs.add(new Folder(name));
    }

    void touch(int size){
        childs.add(new File(name, size));
    }

    void deleteFileSysteNode(IFileSystemNode node){
        childs.remove(node);
    }
    
    IFileSystemNode getChild(String name){
        
    }

    @Override
    public int getSize() {
        int size = 0;
        for(IFileSystemNode node : childs){
            size += node.getSize();
        }
        return size;
    }

    @Override
    public void rename(String name) {
        System.out.println("renamed");
    }

    @Override
    public void printDetails() {
        System.out.println("Current Folder: "+name);
        for(IFileSystemNode node : childs){
            node.printDetails();
        }
    }

}

public class CompositionDesignPatternDemo {
    public static void main(String[] args) {
        Folder root = new Folder("root");
        root.mkdir("movies");
        
    }
}
