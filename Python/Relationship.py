from typing import List


class Player:
    def __init__(self):
        self.name: str = ""
        self.address: str = ""


class Team:
    def __init__(self):
        self.players: List[Player] = []

    def set_player(self, p: "Player") -> None:
        self.players.append(p)  # aggregation
        # self.players.append(Player())  # composition


if __name__ == "__main__":
    pradeep = Player()
    team = Team()
    team.set_player(pradeep)
