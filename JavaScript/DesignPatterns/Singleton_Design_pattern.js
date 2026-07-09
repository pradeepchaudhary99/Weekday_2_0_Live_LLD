class Logger {
    constructor(filePath) {
        console.log("object Created");
        this.filePath = filePath;
    }

    static getInstance() {
        console.log("Thread");
        if (Logger.instance == null) {
            Logger.instance = new Logger("filePath");
        }
        return Logger.instance;
    }
}
Logger.instance = null;

class Singleton_Design_pattern {
    static main(args) {
        const logger = Logger.getInstance();

        const task = () => {
            Logger.getInstance();
        };

        // Node.js has no native multi-threading for plain scripts; JS is
        // single-threaded, so the three "threads" are simulated as direct
        // sequential invocations to preserve the singleton behavior/output.
        const thread1 = task;
        const thread2 = task;
        const thread3 = task;

        thread1();
        thread2();
        thread3();
    }
}

Singleton_Design_pattern.main([]);
