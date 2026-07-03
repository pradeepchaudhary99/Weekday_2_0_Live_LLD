#include <iostream>
#include <vector>
#include <string>

struct Player {
    std::string name;
    std::string address;
};

class Team {
    std::vector<Player*> players;
public:
    void set_player(Player* p) {
        players.push_back(p);  // aggregation
        // players.push_back(new Player());  // composition
    }
};

int main() {
    Player pradeep;
    Team team;
    team.set_player(&pradeep);
    return 0;
}
