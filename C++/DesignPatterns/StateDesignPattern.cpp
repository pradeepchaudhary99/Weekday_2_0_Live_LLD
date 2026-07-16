#include <iostream>
#include <memory>

struct IATMMachineState {
    virtual void insertCard() = 0;
    virtual void withdrawCash() = 0;
    virtual void removeCard() = 0;
    virtual void pressCancel() = 0;
    virtual ~IATMMachineState() = default;
};

class ATMMachine;

// ATM: CardInsertedState, NoMoneyState, HasMoneyState, NoCardState

class CardInsertedState : public IATMMachineState {
public:
    explicit CardInsertedState(ATMMachine* atm) : atm_(atm) {}

    void insertCard() override {
        std::cout << "Already Inserted No use" << std::endl;
    }

    void withdrawCash() override {
        std::cout << "Some 100 Line  of code you are running" << std::endl;
    }

    void removeCard() override;  // defined below, after ATMMachine is complete

    void pressCancel() override {
        removeCard();
    }

private:
    ATMMachine* atm_;
};

class NoCardInsertedState : public IATMMachineState {
public:
    explicit NoCardInsertedState(ATMMachine* atm) : atm_(atm) {}
    void insertCard() override {}
    void withdrawCash() override {}
    void removeCard() override {}
    void pressCancel() override {}

private:
    ATMMachine* atm_;
};

class HasMoneyState : public IATMMachineState {
public:
    explicit HasMoneyState(ATMMachine* atm) : atm_(atm) {}
    void insertCard() override {}
    void withdrawCash() override {}
    void removeCard() override {}
    void pressCancel() override {}

private:
    ATMMachine* atm_;
};

class ATMMachine {
public:
    IATMMachineState* currentState;
    std::unique_ptr<IATMMachineState> cardInsertedState;
    std::unique_ptr<IATMMachineState> noCardInsertedState;
    std::unique_ptr<IATMMachineState> hasMoneyState;

    ATMMachine()
        : cardInsertedState(std::make_unique<CardInsertedState>(this)),
          noCardInsertedState(std::make_unique<NoCardInsertedState>(this)),
          hasMoneyState(std::make_unique<HasMoneyState>(this)) {
        currentState = noCardInsertedState.get();
    }

    void setState(IATMMachineState* state) {
        currentState = state;
    }

    IATMMachineState* getNoCardState() { return noCardInsertedState.get(); }
    IATMMachineState* getCardInsertedState() { return noCardInsertedState.get(); }
    IATMMachineState* getHasMoneyState() { return noCardInsertedState.get(); }

    void insertCard() { currentState->insertCard(); }
    void withdrawCash() { currentState->withdrawCash(); }
    void removeCard() { currentState->removeCard(); }
    void pressCancel() { currentState->pressCancel(); }
};

void CardInsertedState::removeCard() {
    std::cout << "Card is removed" << std::endl;
    atm_->setState(atm_->getNoCardState());  // logic for transitioning the state
}

int main() {
    return 0;
}
