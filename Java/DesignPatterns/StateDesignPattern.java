package DesignPatterns;


interface IATMMachineState{
    void insertCard();
    void withdrawCash();
    void removeCard();
    void pressCancel();
}



//it will have some states 

// ATM: CardInsertedState, NoMoneyState, HasMoneyState, NoCardState

class CardInsertedState implements IATMMachineState{
    private ATMMachine atm;
    
    public CardInsertedState(ATMMachine atm){
        this.atm = atm;
    }
    @Override
    public void insertCard() {
        System.out.println("Already Inserted No use");
    }

    @Override
    public void withdrawCash() {
        System.out.println("Some 100 Line  of code you are running");
    }

    @Override
    public void removeCard() {
        System.out.println("Card is removed");
        atm.setState(atm.getNoCardState()); //logic for transitioning the state
    }

    @Override
    public void pressCancel() {
        removeCard();
    }
}


class No_CardInsertedState implements IATMMachineState{
    private ATMMachine atm;
    public No_CardInsertedState(ATMMachine atm){
        this.atm = atm;
    }
    @Override
    public void insertCard() {

    }

    @Override
    public void withdrawCash() {

    }

    @Override
    public void removeCard() {

    }

    @Override
    public void pressCancel() {

    }
}


class HasMoneyState implements IATMMachineState{
    private ATMMachine atm;
    public HasMoneyState(ATMMachine atm){
        this.atm = atm;
    }
    @Override
    public void insertCard() {

    }

    @Override
    public void withdrawCash() {

    }

    @Override
    public void removeCard() {

    }

    @Override
    public void pressCancel() {

    }
}


class ATMMachine{
    IATMMachineState currentState;
    IATMMachineState cardInsertedState;
    IATMMachineState no_CardInsertedState;
    IATMMachineState hasMoneyState;
    
    public ATMMachine(){
        currentState = new No_CardInsertedState(this);
        cardInsertedState = new CardInsertedState(this);
        no_CardInsertedState = new No_CardInsertedState(this);
        hasMoneyState = new HasMoneyState(this);
    }

    void setState(IATMMachineState state){
        this.currentState = state;
    }

    IATMMachineState getNoCardState(){
        return no_CardInsertedState;
    }
    IATMMachineState getCardInsertedState(){
        return no_CardInsertedState;
    }

    IATMMachineState getHasMoneyState(){
        return no_CardInsertedState;
    }


    void insertCard(){
        currentState.insertCard();
    }
    void withdrawCash(){
        currentState.withdrawCash();
    }
    void removeCard(){
        currentState.removeCard();
    }
    void pressCancel(){
        currentState.pressCancel();
    }


}


public class StateDesignPattern {
    
}
