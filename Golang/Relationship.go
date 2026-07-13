package main

type Player struct {
	Name    string
	Address string
}

type Team struct {
	Players []*Player
}

func (t *Team) SetPlayer(p *Player) {
	t.Players = append(t.Players, p) // aggregation
	// t.Players = append(t.Players, &Player{}) // composition
}

func main() {
	pradeep := &Player{}
	team := &Team{}
	team.SetPlayer(pradeep)
}
