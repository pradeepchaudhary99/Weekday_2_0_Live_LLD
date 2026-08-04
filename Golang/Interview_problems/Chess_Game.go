/*
================================================================================
LLD: Chess Game
================================================================================

Functional Requirements:
    1. Support a standard 8x8 board.
    2. Enforce piece-specific movement and capture rules for every piece type
       (KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN).
    3. Detect check, checkmate, and stalemate after every move.
    4. Maintain move history to support undo.

Non-Functional Requirements:
    1. Move validation latency should be small: each call generates a
       piece's candidate moves in O(board size), never scanning the whole
       game tree.
    2. Extensibility: adding a new piece type or a variant rule (e.g.
       castling) should not require touching unrelated piece types.

--------------------------------------------------------------------------
Design
--------------------------------------------------------------------------
Strategy pattern for movement:
    Every piece knows only its OWN movement rule (Piece.GetPossibleMoves).
    Board/Game never special-case "if this is a bishop do X" -- they just
    call piece.GetPossibleMoves(...) through the Piece interface. Adding a
    new piece type means adding one new type that satisfies the interface,
    nothing else changes. (Go has no inheritance, so unlike the Java/C++/
    Python versions, GetAttackedSquares is implemented once per type rather
    than defaulting through a base class -- but the intent is identical:
    every piece but the pawn just delegates to GetPossibleMoves.)

Command-style Move records for undo:
    Each executed Move records exactly what it takes to reverse itself: the
    piece that was AT the source square before the move (so promotion can
    be undone back to a pawn) and whatever piece occupied the destination
    square before it was captured (possibly nil). Undo() pops the last
    Move and replays those two facts backwards -- it never needs to
    recompute anything from scratch.

Core Entities:
    Game             -- orchestrates turns, applies/undoes moves, tracks status
    Board            -- 8x8 grid of Piece references + board-level queries
    Piece (interface) -- Color, Type, GetPossibleMoves() [King, Queen,
                         Rook, Bishop, Knight, Pawn]
    Move             -- Source, Destination, PieceMoved (pre-move), CapturedPiece
    Player           -- Name + Color
    GameStatus       -- Ongoing, Check, Checkmate, Stalemate

Out of scope: castling, en passant, draw by repetition/50-move rule.
================================================================================
*/

package main

import (
	"fmt"
	"strings"
)

// Position is (Row, Col); row 0 = rank 1 (White's back rank), col 0 = file 'a'.
type Position struct {
	Row, Col int
}

type Color int

const (
	White Color = iota
	Black
)

func (c Color) Opposite() Color {
	if c == White {
		return Black
	}
	return White
}

type PieceType int

const (
	KingType PieceType = iota
	QueenType
	RookType
	BishopType
	KnightType
	PawnType
)

type GameStatus int

const (
	Ongoing GameStatus = iota
	Check
	Checkmate
	Stalemate
)

func (s GameStatus) String() string {
	switch s {
	case Ongoing:
		return "ONGOING"
	case Check:
		return "CHECK"
	case Checkmate:
		return "CHECKMATE"
	case Stalemate:
		return "STALEMATE"
	}
	return "UNKNOWN"
}

// Piece is the strategy interface every piece type implements.
type Piece interface {
	Color() Color
	Type() PieceType
	// GetPossibleMoves returns pseudo-legal moves: obeys this piece's
	// movement/blocking/capture rules, but ignores whether the move leaves
	// the mover's own king in check -- Game.Move() layers that safety
	// check on top.
	GetPossibleMoves(position Position, board *Board) []Position
	// GetAttackedSquares returns the squares this piece threatens; used
	// for check detection. Identical to GetPossibleMoves for every piece
	// except the pawn, whose diagonal attacks threaten a square whether or
	// not an enemy is standing on it yet.
	GetAttackedSquares(position Position, board *Board) []Position
	Symbol() byte
}

type basePiece struct {
	color     Color
	pieceType PieceType
}

func (p *basePiece) Color() Color    { return p.color }
func (p *basePiece) Type() PieceType { return p.pieceType }
func (p *basePiece) symbolFor(letter byte) byte {
	if p.color == White {
		return letter
	}
	return letter + ('a' - 'A')
}

// slide is the shared helper for the sliding pieces (rook/bishop/queen):
// walk each direction until the edge of the board, a friendly piece (stop
// before it), or an enemy piece (stop after capturing it).
func slide(position Position, board *Board, color Color, directions [][2]int) []Position {
	var moves []Position
	for _, d := range directions {
		r, c := position.Row+d[0], position.Col+d[1]
		for board.IsValidCell(r, c) {
			occupant := board.GetPiece(r, c)
			if occupant == nil {
				moves = append(moves, Position{r, c})
			} else {
				if occupant.Color() != color {
					moves = append(moves, Position{r, c})
				}
				break
			}
			r += d[0]
			c += d[1]
		}
	}
	return moves
}

type Rook struct{ basePiece }

func NewRook(color Color) *Rook { return &Rook{basePiece{color, RookType}} }
func (r *Rook) GetPossibleMoves(position Position, board *Board) []Position {
	return slide(position, board, r.color, [][2]int{{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
}
func (r *Rook) GetAttackedSquares(position Position, board *Board) []Position {
	return r.GetPossibleMoves(position, board)
}
func (r *Rook) Symbol() byte { return r.symbolFor('R') }

type Bishop struct{ basePiece }

func NewBishop(color Color) *Bishop { return &Bishop{basePiece{color, BishopType}} }
func (b *Bishop) GetPossibleMoves(position Position, board *Board) []Position {
	return slide(position, board, b.color, [][2]int{{1, 1}, {1, -1}, {-1, 1}, {-1, -1}})
}
func (b *Bishop) GetAttackedSquares(position Position, board *Board) []Position {
	return b.GetPossibleMoves(position, board)
}
func (b *Bishop) Symbol() byte { return b.symbolFor('B') }

type Queen struct{ basePiece }

func NewQueen(color Color) *Queen { return &Queen{basePiece{color, QueenType}} }
func (q *Queen) GetPossibleMoves(position Position, board *Board) []Position {
	return slide(position, board, q.color, [][2]int{
		{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
	})
}
func (q *Queen) GetAttackedSquares(position Position, board *Board) []Position {
	return q.GetPossibleMoves(position, board)
}
func (q *Queen) Symbol() byte { return q.symbolFor('Q') }

var knightOffsets = [][2]int{{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}}

type Knight struct{ basePiece }

func NewKnight(color Color) *Knight { return &Knight{basePiece{color, KnightType}} }
func (k *Knight) GetPossibleMoves(position Position, board *Board) []Position {
	var moves []Position
	for _, d := range knightOffsets {
		r, c := position.Row+d[0], position.Col+d[1]
		if board.IsValidCell(r, c) {
			occupant := board.GetPiece(r, c)
			if occupant == nil || occupant.Color() != k.color {
				moves = append(moves, Position{r, c})
			}
		}
	}
	return moves
}
func (k *Knight) GetAttackedSquares(position Position, board *Board) []Position {
	return k.GetPossibleMoves(position, board)
}
func (k *Knight) Symbol() byte { return k.symbolFor('N') }

type King struct{ basePiece }

func NewKing(color Color) *King { return &King{basePiece{color, KingType}} }
func (k *King) GetPossibleMoves(position Position, board *Board) []Position {
	var moves []Position
	for dRow := -1; dRow <= 1; dRow++ {
		for dCol := -1; dCol <= 1; dCol++ {
			if dRow == 0 && dCol == 0 {
				continue
			}
			r, c := position.Row+dRow, position.Col+dCol
			if board.IsValidCell(r, c) {
				occupant := board.GetPiece(r, c)
				if occupant == nil || occupant.Color() != k.color {
					moves = append(moves, Position{r, c})
				}
			}
		}
	}
	return moves
}
func (k *King) GetAttackedSquares(position Position, board *Board) []Position {
	return k.GetPossibleMoves(position, board)
}
func (k *King) Symbol() byte { return k.symbolFor('K') }

type Pawn struct{ basePiece }

func NewPawn(color Color) *Pawn { return &Pawn{basePiece{color, PawnType}} }
func (p *Pawn) GetPossibleMoves(position Position, board *Board) []Position {
	direction := -1
	startRow := 6
	if p.color == White {
		direction = 1
		startRow = 1
	}
	var moves []Position

	oneStep := Position{position.Row + direction, position.Col}
	if board.IsValidCell(oneStep.Row, oneStep.Col) && board.GetPiece(oneStep.Row, oneStep.Col) == nil {
		moves = append(moves, oneStep)
		twoStep := Position{position.Row + 2*direction, position.Col}
		if position.Row == startRow && board.GetPiece(twoStep.Row, twoStep.Col) == nil {
			moves = append(moves, twoStep)
		}
	}

	for _, dCol := range []int{-1, 1} {
		r, c := position.Row+direction, position.Col+dCol
		if board.IsValidCell(r, c) {
			occupant := board.GetPiece(r, c)
			if occupant != nil && occupant.Color() != p.color {
				moves = append(moves, Position{r, c})
			}
		}
	}
	return moves
}

// GetAttackedSquares is the one place a pawn diverges from GetPossibleMoves:
// its diagonal attacks threaten a square whether or not an enemy currently
// occupies it, which matters for detecting check.
func (p *Pawn) GetAttackedSquares(position Position, board *Board) []Position {
	direction := -1
	if p.color == White {
		direction = 1
	}
	var squares []Position
	for _, dCol := range []int{-1, 1} {
		r, c := position.Row+direction, position.Col+dCol
		if board.IsValidCell(r, c) {
			squares = append(squares, Position{r, c})
		}
	}
	return squares
}
func (p *Pawn) Symbol() byte { return p.symbolFor('P') }

const BoardSize = 8

type Board struct {
	cells [BoardSize][BoardSize]Piece
}

func NewBoard() *Board {
	b := &Board{}
	b.Initialize()
	return b
}

func (b *Board) Initialize() {
	backRank := []PieceType{RookType, KnightType, BishopType, QueenType, KingType, BishopType, KnightType, RookType}
	for col, pt := range backRank {
		b.cells[0][col] = makePiece(pt, White)
		b.cells[7][col] = makePiece(pt, Black)
	}
	for col := 0; col < BoardSize; col++ {
		b.cells[1][col] = NewPawn(White)
		b.cells[6][col] = NewPawn(Black)
	}
}

func makePiece(pt PieceType, color Color) Piece {
	switch pt {
	case RookType:
		return NewRook(color)
	case KnightType:
		return NewKnight(color)
	case BishopType:
		return NewBishop(color)
	case QueenType:
		return NewQueen(color)
	case KingType:
		return NewKing(color)
	}
	panic("unsupported back-rank piece type")
}

func (b *Board) IsValidCell(row, col int) bool {
	return row >= 0 && row < BoardSize && col >= 0 && col < BoardSize
}

func (b *Board) GetPiece(row, col int) Piece { return b.cells[row][col] }

func (b *Board) SetPiece(position Position, piece Piece) { b.cells[position.Row][position.Col] = piece }

func (b *Board) MovePiece(source, destination Position) Piece {
	captured := b.GetPiece(destination.Row, destination.Col)
	b.SetPiece(destination, b.GetPiece(source.Row, source.Col))
	b.SetPiece(source, nil)
	return captured
}

func (b *Board) FindKing(color Color) (Position, bool) {
	for row := 0; row < BoardSize; row++ {
		for col := 0; col < BoardSize; col++ {
			p := b.cells[row][col]
			if p != nil && p.Type() == KingType && p.Color() == color {
				return Position{row, col}, true
			}
		}
	}
	return Position{}, false
}

type pieceAt struct {
	Position Position
	Piece    Piece
}

func (b *Board) AllPieces(color Color) []pieceAt {
	var result []pieceAt
	for row := 0; row < BoardSize; row++ {
		for col := 0; col < BoardSize; col++ {
			p := b.cells[row][col]
			if p != nil && p.Color() == color {
				result = append(result, pieceAt{Position{row, col}, p})
			}
		}
	}
	return result
}

func (b *Board) PrintBoard() {
	for row := BoardSize - 1; row >= 0; row-- {
		var sb strings.Builder
		fmt.Fprintf(&sb, "%d ", row+1)
		for col := 0; col < BoardSize; col++ {
			p := b.cells[row][col]
			if p != nil {
				sb.WriteByte(p.Symbol())
			} else {
				sb.WriteByte('.')
			}
			if col != BoardSize-1 {
				sb.WriteByte(' ')
			}
		}
		fmt.Println(sb.String())
	}
	fmt.Println("  a b c d e f g h")
}

// Move is a self-contained "command": it records the two facts needed to
// reverse itself (PieceMoved -- as it was BEFORE this move, so a promoted
// queen can be undone back into a pawn -- and CapturedPiece, possibly
// nil), so Game.Undo() never has to recompute board state from scratch.
type Move struct {
	Source        Position
	Destination   Position
	PieceMoved    Piece
	CapturedPiece Piece
}

type Player struct {
	Name  string
	Color Color
}

func parseSquare(square string) Position {
	col := int(square[0] - 'a')
	row := int(square[1] - '1')
	return Position{row, col}
}

type Game struct {
	board         *Board
	white         *Player
	black         *Player
	CurrentPlayer *Player
	Status        GameStatus
	// history is the undo stack: the last move played sits at the end,
	// ready to be reversed.
	history []Move
}

func NewGame(whiteName, blackName string) *Game {
	white := &Player{whiteName, White}
	black := &Player{blackName, Black}
	return &Game{
		board:         NewBoard(),
		white:         white,
		black:         black,
		CurrentPlayer: white,
		Status:        Ongoing,
	}
}

func (g *Game) SwitchTurn() {
	if g.CurrentPlayer == g.white {
		g.CurrentPlayer = g.black
	} else {
		g.CurrentPlayer = g.white
	}
}

func (g *Game) IsSquareAttacked(position Position, byColor Color) bool {
	for _, pa := range g.board.AllPieces(byColor) {
		for _, attacked := range pa.Piece.GetAttackedSquares(pa.Position, g.board) {
			if attacked == position {
				return true
			}
		}
	}
	return false
}

func (g *Game) IsCheck(color Color) bool {
	kingPos, ok := g.board.FindKing(color)
	if !ok {
		return false
	}
	return g.IsSquareAttacked(kingPos, color.Opposite())
}

func (g *Game) hasLegalMove(color Color) bool {
	for _, pa := range g.board.AllPieces(color) {
		for _, destination := range pa.Piece.GetPossibleMoves(pa.Position, g.board) {
			captured := g.board.MovePiece(pa.Position, destination)
			stillInCheck := g.IsCheck(color)
			g.board.SetPiece(pa.Position, pa.Piece)
			g.board.SetPiece(destination, captured)
			if !stillInCheck {
				return true
			}
		}
	}
	return false
}

func (g *Game) IsCheckmate(color Color) bool { return g.IsCheck(color) && !g.hasLegalMove(color) }
func (g *Game) IsStalemate(color Color) bool { return !g.IsCheck(color) && !g.hasLegalMove(color) }

func (g *Game) Move(source, destination Position) (Move, error) {
	if g.Status == Checkmate || g.Status == Stalemate {
		return Move{}, fmt.Errorf("game is already over")
	}

	piece := g.board.GetPiece(source.Row, source.Col)
	if piece == nil || piece.Color() != g.CurrentPlayer.Color {
		return Move{}, fmt.Errorf("no movable piece of the current player at source")
	}

	legal := false
	for _, candidate := range piece.GetPossibleMoves(source, g.board) {
		if candidate == destination {
			legal = true
			break
		}
	}
	if !legal {
		return Move{}, fmt.Errorf("illegal move for this piece")
	}

	captured := g.board.MovePiece(source, destination)
	if g.IsCheck(g.CurrentPlayer.Color) {
		// Undo the tentative move: it would leave (or keep) our own king in check.
		g.board.SetPiece(source, piece)
		g.board.SetPiece(destination, captured)
		return Move{}, fmt.Errorf("move leaves own king in check")
	}

	// The Move record keeps `piece` (pre-promotion) as PieceMoved so Undo()
	// can restore the pawn even after we replace the board occupant with a
	// queen below.
	performedMove := Move{source, destination, piece, captured}
	g.history = append(g.history, performedMove)

	if piece.Type() == PawnType && (destination.Row == 0 || destination.Row == BoardSize-1) {
		g.board.SetPiece(destination, NewQueen(piece.Color()))
	}

	g.SwitchTurn()
	g.recomputeStatus()
	return performedMove, nil
}

// Undo pops the most recent move and restores the board exactly as it was
// before that move, including undoing promotions (PieceMoved is always the
// pre-promotion piece) and restoring captures.
func (g *Game) Undo() error {
	if len(g.history) == 0 {
		return fmt.Errorf("no moves to undo")
	}
	last := g.history[len(g.history)-1]
	g.history = g.history[:len(g.history)-1]
	g.board.SetPiece(last.Source, last.PieceMoved)
	g.board.SetPiece(last.Destination, last.CapturedPiece)
	g.SwitchTurn()
	g.recomputeStatus()
	return nil
}

func (g *Game) recomputeStatus() {
	toMove := g.CurrentPlayer.Color
	if g.IsCheckmate(toMove) {
		g.Status = Checkmate
	} else if g.IsCheck(toMove) {
		g.Status = Check
	} else if g.IsStalemate(toMove) {
		g.Status = Stalemate
	} else {
		g.Status = Ongoing
	}
}

func main() {
	game := NewGame("Alice", "Bob")

	// Fool's Mate -- fastest possible checkmate, four half-moves.
	moves := []string{"f2f3", "e7e5", "g2g4", "d8h4"}
	for _, moveStr := range moves {
		source := parseSquare(moveStr[0:2])
		destination := parseSquare(moveStr[2:4])
		mover := game.CurrentPlayer.Name
		if _, err := game.Move(source, destination); err != nil {
			panic(err)
		}
		fmt.Printf("%s played %s -> status: %s\n", mover, moveStr, game.Status)
	}

	fmt.Println()
	game.board.PrintBoard()
	fmt.Printf("\nStatus before undo: %s\n", game.Status)

	fmt.Println("\n-- Undoing the checkmating move --")
	if err := game.Undo(); err != nil {
		panic(err)
	}
	fmt.Printf("Status after undo: %s (back to move for %s)\n", game.Status, game.CurrentPlayer.Name)
	game.board.PrintBoard()
}
