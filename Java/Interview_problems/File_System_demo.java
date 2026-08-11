

/*
1. user should be able to createDirectory(path)
2. createFile(path)
3. writeFile(path, content)
4. readFile(path)
5. delete(path)
6. calculate the size of a file/folder 
7. move(source, destination path)
8. Permissions at directory Level for various types of users [ADMIN, USER]


Core Entities:
FileSystemNode 
File 
Directory 
FileSystemManager


*/

abstract class FileSystemNode{
    String name;
    Directory directory;
    getName();
    getSize();
    getParent();
    boolean isDirectory();

    FileSystemNode(String name, Directory parent){
        this.name = name;
        this.parent = parent;
    }
}

class File extends FileSystemNode{
    String content;

    String read();
    void write(String content);
    void append(String content);
    isDirectory(){
        return false;
    }
}

class Directory extends FileSystemNode{
    Map<String, FileSystemNode> children;

    isDirectory(){
        return true;
    }
    addChild(FileSystemNode child);
    getChild(String name);
    removeChild(String name);
}

class FileSystemManager{
    Directory root;
    Directory currentDirectory;
    // 1. user should be able to createDirectory(path)
    // 2. createFile(path)
    // 3. writeFile(path, content)
    // 4. readFile(path)
    // 5. delete(path)
    // 6. calculate the size of a file/folder 
    // 7. move(source, destination path)


    void createFile(String path){

        Directory current = currentDirectory;
        String [] pathResolved = path.split("/"); ["","a","b","c","d","e","f.txt"]
        
        for(int i = 1; i < pathResolved.length; i++){
            if(!current.children.containsKey(pathResolved[i])){
                return false; // error invalid path..
            }else{
                current = current.get(pathResolved[i]);
            }
        }

    }
}


public class File_System_demo {
    
}
