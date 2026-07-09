class Player {
    constructor() {
        this.name = undefined;
        this.address = undefined;
    }
}

class Team {
    constructor() {
        this.players = undefined; // List<Player> players; (never initialized, mirrors Java field default null)
    }

    addPlayers(p) {
        this.players.add(p);
    }
    removePlayer(p) {
        this.players.remove(p);
    }
}

class SponsorCompany {
    constructor() {
        this.player = undefined;
    }

    hirePlayer(p) {
        this.player = p;
    }
}

// Composition

class File {
    constructor() {
        this.fileName = undefined;
        this.content = undefined;
        this.size = undefined;
    }
}

class Folder {
    constructor() {
        this.files = undefined; // List<File> files; (never initialized, mirrors Java field default null)
    }

    touch(fileName) {
        this.files.add(new File(fileName));
    }
}
