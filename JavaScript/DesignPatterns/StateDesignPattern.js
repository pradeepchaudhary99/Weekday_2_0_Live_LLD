'use strict';

class IATMMachineState {
    insertCard() {
        throw new Error("Not implemented");
    }

    withdrawCash() {
        throw new Error("Not implemented");
    }

    removeCard() {
        throw new Error("Not implemented");
    }

    pressCancel() {
        throw new Error("Not implemented");
    }
}

// ATM: CardInsertedState, NoMoneyState, HasMoneyState, NoCardState

class CardInsertedState extends IATMMachineState {
    constructor(atm) {
        super();
        this.atm = atm;
    }

    insertCard() {
        console.log("Already Inserted No use");
    }

    withdrawCash() {
        console.log("Some 100 Line  of code you are running");
    }

    removeCard() {
        console.log("Card is removed");
        this.atm.setState(this.atm.getNoCardState());  // logic for transitioning the state
    }

    pressCancel() {
        this.removeCard();
    }
}

class NoCardInsertedState extends IATMMachineState {
    constructor(atm) {
        super();
        this.atm = atm;
    }

    insertCard() {}

    withdrawCash() {}

    removeCard() {}

    pressCancel() {}
}

class HasMoneyState extends IATMMachineState {
    constructor(atm) {
        super();
        this.atm = atm;
    }

    insertCard() {}

    withdrawCash() {}

    removeCard() {}

    pressCancel() {}
}

class ATMMachine {
    constructor() {
        this.noCardInsertedState = new NoCardInsertedState(this);
        this.cardInsertedState = new CardInsertedState(this);
        this.hasMoneyState = new HasMoneyState(this);
        this.currentState = this.noCardInsertedState;
    }

    setState(state) {
        this.currentState = state;
    }

    getNoCardState() {
        return this.noCardInsertedState;
    }

    getCardInsertedState() {
        return this.noCardInsertedState;
    }

    getHasMoneyState() {
        return this.noCardInsertedState;
    }

    insertCard() {
        this.currentState.insertCard();
    }

    withdrawCash() {
        this.currentState.withdrawCash();
    }

    removeCard() {
        this.currentState.removeCard();
    }

    pressCancel() {
        this.currentState.pressCancel();
    }
}
