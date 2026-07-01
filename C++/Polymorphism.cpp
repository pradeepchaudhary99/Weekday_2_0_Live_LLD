#include <string>
#include <vector>
#include <algorithm>


struct Player {
    std::string name;
    std::string address;
};

struct Team {
    std::vector<Player> players;

    void add_player(const Player& p) {
        players.push_back(p);
    }

    void remove_player(const Player& p) {
        players.erase(
            std::remove_if(players.begin(), players.end(),
                [&](const Player& x) { return x.name == p.name && x.address == p.address; }),
            players.end()
        );
    }
};

struct SponsorCompany {
    Player* player = nullptr;

    void hire_player(Player& p) {
        player = &p;
    }
};


// Composition

struct File {
    std::string file_name;
    std::string content;
    int size = 0;
};

struct Folder {
    std::vector<File> files;

    void touch(const std::string& file_name) {
        files.push_back(File{file_name});
    }
};
