'use strict';

class Request {
    constructor(priority = 0) {
        this.priority = priority;
    }
}

class IHandler {
    constructor() {
        this.nextHandler = null;
    }

    setNextHandler(handler) {
        this.nextHandler = handler;
        return this.nextHandler;
    }

    getNextHandler() {
        return this.nextHandler;
    }

    isAllowed(request) {
        throw new Error("Not implemented");
    }

    processRequest(request) {
        throw new Error("Not implemented");
    }
}

class Level1Handler extends IHandler {
    isAllowed(request) {
        return request.priority < 8;
    }

    processRequest(request) {
        if (this.isAllowed(request)) {
            console.log("Processing the request : lvel1 is processing");
        } else {
            this.getNextHandler().processRequest(request);
        }
    }
}

class Level2Handler extends IHandler {
    isAllowed(request) {
        return request.priority < 15;
    }

    processRequest(request) {
        if (this.isAllowed(request)) {
            console.log("Processing the request : lvel1 is processing");
        } else {
            this.getNextHandler().processRequest(request);
        }
    }
}

class Level3Handler extends IHandler {
    isAllowed(request) {
        return request.priority < 2;
    }

    processRequest(request) {
        if (this.isAllowed(request)) {
            console.log("Processing the request : lvel1 is processing");
        } else {
            this.getNextHandler().processRequest(request);
        }
    }
}

const level1 = new Level1Handler();
const level2 = new Level2Handler();
const level3 = new Level3Handler();

level1.setNextHandler(level2).setNextHandler(level3);
