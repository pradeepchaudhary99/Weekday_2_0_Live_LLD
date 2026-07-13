import java.util.List;

class Player{
    String name;
    String address;
}

class Team{
    List<Player> players;

    void addPlayers(Player p){
        players.add(p);
    }
    void removePlayer(Player p){
        players.remove(p);
    }
}


class SponsorCompany{
    Player player;

    void hirePlayer(Player p){
        this.player = p;
    }
}



//Composition


class File{
    String fileName;
    String content;
    int size;
}

class Folder{
    List<File> files;

    void touch(String fileName){
        files.add(new File(fileName));
    }
}