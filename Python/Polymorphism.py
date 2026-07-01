from typing import List, Optional
from dataclasses import dataclass, field


@dataclass
class Player:
    name: str = ""
    address: str = ""


@dataclass
class Team:
    players: List[Player] = field(default_factory=list)

    def add_player(self, p: Player) -> None:
        self.players.append(p)

    def remove_player(self, p: Player) -> None:
        self.players.remove(p)


@dataclass
class SponsorCompany:
    player: Optional[Player] = None

    def hire_player(self, p: Player) -> None:
        self.player = p


# Composition

@dataclass
class File:
    file_name: str = ""
    content: str = ""
    size: int = 0


@dataclass
class Folder:
    files: List[File] = field(default_factory=list)

    def touch(self, file_name: str) -> None:
        self.files.append(File(file_name))
