package main

type Player struct {
	Name    string
	Address string
}

type Team struct {
	Players []*Player
}

func (t *Team) AddPlayers(p *Player) {
	t.Players = append(t.Players, p)
}

func (t *Team) RemovePlayer(p *Player) {
	for i, player := range t.Players {
		if player == p {
			t.Players = append(t.Players[:i], t.Players[i+1:]...)
			break
		}
	}
}

type SponsorCompany struct {
	Player *Player
}

func (s *SponsorCompany) HirePlayer(p *Player) {
	s.Player = p
}

// Composition

type File struct {
	FileName string
	Content  string
	Size     int
}

type Folder struct {
	Files []*File
}

func (fo *Folder) Touch(fileName string) {
	fo.Files = append(fo.Files, &File{FileName: fileName})
}

func main() {}
