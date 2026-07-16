from abc import ABC, abstractmethod


class IATMMachineState(ABC):
    @abstractmethod
    def insert_card(self) -> None:
        ...

    @abstractmethod
    def withdraw_cash(self) -> None:
        ...

    @abstractmethod
    def remove_card(self) -> None:
        ...

    @abstractmethod
    def press_cancel(self) -> None:
        ...


# ATM: CardInsertedState, NoMoneyState, HasMoneyState, NoCardState


class CardInsertedState(IATMMachineState):
    def __init__(self, atm: "ATMMachine") -> None:
        self.atm = atm

    def insert_card(self) -> None:
        print("Already Inserted No use")

    def withdraw_cash(self) -> None:
        print("Some 100 Line  of code you are running")

    def remove_card(self) -> None:
        print("Card is removed")
        self.atm.set_state(self.atm.get_no_card_state())  # logic for transitioning the state

    def press_cancel(self) -> None:
        self.remove_card()


class NoCardInsertedState(IATMMachineState):
    def __init__(self, atm: "ATMMachine") -> None:
        self.atm = atm

    def insert_card(self) -> None:
        pass

    def withdraw_cash(self) -> None:
        pass

    def remove_card(self) -> None:
        pass

    def press_cancel(self) -> None:
        pass


class HasMoneyState(IATMMachineState):
    def __init__(self, atm: "ATMMachine") -> None:
        self.atm = atm

    def insert_card(self) -> None:
        pass

    def withdraw_cash(self) -> None:
        pass

    def remove_card(self) -> None:
        pass

    def press_cancel(self) -> None:
        pass


class ATMMachine:
    def __init__(self) -> None:
        self.no_card_inserted_state: IATMMachineState = NoCardInsertedState(self)
        self.card_inserted_state: IATMMachineState = CardInsertedState(self)
        self.has_money_state: IATMMachineState = HasMoneyState(self)
        self.current_state: IATMMachineState = self.no_card_inserted_state

    def set_state(self, state: IATMMachineState) -> None:
        self.current_state = state

    def get_no_card_state(self) -> IATMMachineState:
        return self.no_card_inserted_state

    def get_card_inserted_state(self) -> IATMMachineState:
        return self.no_card_inserted_state

    def get_has_money_state(self) -> IATMMachineState:
        return self.no_card_inserted_state

    def insert_card(self) -> None:
        self.current_state.insert_card()

    def withdraw_cash(self) -> None:
        self.current_state.withdraw_cash()

    def remove_card(self) -> None:
        self.current_state.remove_card()

    def press_cancel(self) -> None:
        self.current_state.press_cancel()


def main() -> None:
    pass


if __name__ == "__main__":
    main()
