class Player {
    constructor() {
        this.name = undefined;
        this.address = undefined;
    }
}

class Team {
    constructor() {
        this.players = undefined; // List<Player> players; (never initialized)
    }

    setPlayer(p) {
        this.players.add(p); // aggregation
        // this.players.add(new Player()); // composition
    }
}

class Relationship {
    static main(args) {
        const pradeep = new Player();
        const team = new Team();
        // NOTE: pre-existing bug preserved from Java source — the original
        // `team.se` was an incomplete/truncated statement (a Java compile
        // error). Translated here as a call to a non-existent method so it
        // fails at runtime (TypeError) rather than introducing a JS syntax
        // error, matching the intent of "faithfully broken" original code.
        team.se();
    }
}

Relationship.main([]);
