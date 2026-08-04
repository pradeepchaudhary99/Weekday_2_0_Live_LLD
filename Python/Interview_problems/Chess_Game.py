"""
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
       castling) should not require touching unrelated piece classes.

--------------------------------------------------------------------------
Design
--------------------------------------------------------------------------
Strategy pattern for movement:
    Every piece knows only its OWN movement rule (Piece.get_possible_moves).
    Board/Game never special-case "if this is a bishop do X" -- they just
    call piece.get_possible_moves(...) polymorphically. Adding a new piece
    type means adding one new subclass, nothing else changes.

Command-style Move objects for undo:
    Each executed Move records exactly what it takes to reverse itself: the
    piece that was AT the source square before the move (so promotion can
    be undone back to a pawn) and whatever piece occupied the destination
    square before it was captured (possibly None). undo() pops the last
    Move and replays those two facts backwards -- it never needs to
    recompute anything from scratch.

Core Entities:
    Game             -- orchestrates turns, applies/undoes moves, tracks status
    Board            -- 8x8 grid of Piece references + board-level queries
    Piece (abstract)  -- Color, PieceType, get_possible_moves() [King, Queen,
                         Rook, Bishop, Knight, Pawn]
    Move             -- source, destination, piece_moved (pre-move), captured_piece
    Player           -- name + color
    GameStatus       -- ONGOING, CHECK, CHECKMATE, STALEMATE

Out of scope: castling, en passant, draw by repetition/50-move rule.
================================================================================
"""

from abc import ABC, abstractmethod
from enum import Enum, auto
from typing import List, Optional, Tuple

Position = Tuple[int, int]  # (row, col); row 0 = rank 1 (White's back rank), col 0 = file 'a'.


class Color(Enum):
    WHITE = auto()
    BLACK = auto()

    def opposite(self) -> "Color":
        return Color.BLACK if self == Color.WHITE else Color.WHITE


class PieceType(Enum):
    KING = auto()
    QUEEN = auto()
    ROOK = auto()
    BISHOP = auto()
    KNIGHT = auto()
    PAWN = auto()


class GameStatus(Enum):
    ONGOING = auto()
    CHECK = auto()
    CHECKMATE = auto()
    STALEMATE = auto()


class Piece(ABC):
    def __init__(self, color: Color, piece_type: PieceType):
        self.color = color
        self.type = piece_type

    @abstractmethod
    def get_possible_moves(self, position: Position, board: "Board") -> List[Position]:
        """Pseudo-legal moves: obeys this piece's movement/blocking/capture
        rules, but ignores whether the move leaves the mover's own king in
        check -- Game.move() layers that safety check on top."""
        raise NotImplementedError

    def get_attacked_squares(self, position: Position, board: "Board") -> List[Position]:
        """Squares this piece threatens; used for check detection. Identical
        to get_possible_moves for every piece except the pawn, whose
        diagonal attacks threaten a square whether or not an enemy is
        standing on it yet (a plain move-list wouldn't include an empty
        diagonal square)."""
        return self.get_possible_moves(position, board)

    def symbol(self) -> str:
        letters = {
            PieceType.KING: "K", PieceType.QUEEN: "Q", PieceType.ROOK: "R",
            PieceType.BISHOP: "B", PieceType.KNIGHT: "N", PieceType.PAWN: "P",
        }
        letter = letters[self.type]
        return letter if self.color == Color.WHITE else letter.lower()

    def _slide(self, position: Position, board: "Board", directions: List[Position]) -> List[Position]:
        """Shared helper for the sliding pieces (rook/bishop/queen): walk
        each direction until the edge of the board, a friendly piece (stop
        before it), or an enemy piece (stop after capturing it)."""
        moves = []
        row, col = position
        for d_row, d_col in directions:
            r, c = row + d_row, col + d_col
            while board.is_valid_cell((r, c)):
                occupant = board.get_piece((r, c))
                if occupant is None:
                    moves.append((r, c))
                else:
                    if occupant.color != self.color:
                        moves.append((r, c))
                    break
                r, c = r + d_row, c + d_col
        return moves


class Rook(Piece):
    def __init__(self, color: Color):
        super().__init__(color, PieceType.ROOK)

    def get_possible_moves(self, position: Position, board: "Board") -> List[Position]:
        return self._slide(position, board, [(1, 0), (-1, 0), (0, 1), (0, -1)])


class Bishop(Piece):
    def __init__(self, color: Color):
        super().__init__(color, PieceType.BISHOP)

    def get_possible_moves(self, position: Position, board: "Board") -> List[Position]:
        return self._slide(position, board, [(1, 1), (1, -1), (-1, 1), (-1, -1)])


class Queen(Piece):
    def __init__(self, color: Color):
        super().__init__(color, PieceType.QUEEN)

    def get_possible_moves(self, position: Position, board: "Board") -> List[Position]:
        return self._slide(position, board, [
            (1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1),
        ])


class Knight(Piece):
    def __init__(self, color: Color):
        super().__init__(color, PieceType.KNIGHT)

    def get_possible_moves(self, position: Position, board: "Board") -> List[Position]:
        row, col = position
        moves = []
        for d_row, d_col in [(2, 1), (2, -1), (-2, 1), (-2, -1), (1, 2), (1, -2), (-1, 2), (-1, -2)]:
            candidate = (row + d_row, col + d_col)
            if board.is_valid_cell(candidate):
                occupant = board.get_piece(candidate)
                if occupant is None or occupant.color != self.color:
                    moves.append(candidate)
        return moves


class King(Piece):
    def __init__(self, color: Color):
        super().__init__(color, PieceType.KING)

    def get_possible_moves(self, position: Position, board: "Board") -> List[Position]:
        row, col = position
        moves = []
        for d_row in (-1, 0, 1):
            for d_col in (-1, 0, 1):
                if d_row == 0 and d_col == 0:
                    continue
                candidate = (row + d_row, col + d_col)
                if board.is_valid_cell(candidate):
                    occupant = board.get_piece(candidate)
                    if occupant is None or occupant.color != self.color:
                        moves.append(candidate)
        return moves


class Pawn(Piece):
    def __init__(self, color: Color):
        super().__init__(color, PieceType.PAWN)

    def get_possible_moves(self, position: Position, board: "Board") -> List[Position]:
        row, col = position
        direction = 1 if self.color == Color.WHITE else -1
        start_row = 1 if self.color == Color.WHITE else 6
        moves = []

        one_step = (row + direction, col)
        if board.is_valid_cell(one_step) and board.get_piece(one_step) is None:
            moves.append(one_step)
            two_step = (row + 2 * direction, col)
            if row == start_row and board.get_piece(two_step) is None:
                moves.append(two_step)

        for d_col in (-1, 1):
            capture = (row + direction, col + d_col)
            if board.is_valid_cell(capture):
                occupant = board.get_piece(capture)
                if occupant is not None and occupant.color != self.color:
                    moves.append(capture)
        return moves

    def get_attacked_squares(self, position: Position, board: "Board") -> List[Position]:
        row, col = position
        direction = 1 if self.color == Color.WHITE else -1
        squares = []
        for d_col in (-1, 1):
            candidate = (row + direction, col + d_col)
            if board.is_valid_cell(candidate):
                squares.append(candidate)
        return squares


class Move:
    """A self-contained "command": records the two facts needed to reverse
    itself (piece_moved -- as it was BEFORE this move, so a promoted queen
    can be undone back into a pawn -- and captured_piece, possibly None),
    so Game.undo() never has to recompute board state from scratch."""

    def __init__(self, source: Position, destination: Position,
                 piece_moved: Piece, captured_piece: Optional[Piece]):
        self.source = source
        self.destination = destination
        self.piece_moved = piece_moved
        self.captured_piece = captured_piece


class Board:
    SIZE = 8

    def __init__(self):
        self.cells: List[List[Optional[Piece]]] = [[None] * Board.SIZE for _ in range(Board.SIZE)]
        self.initialize()

    def initialize(self) -> None:
        back_rank = [PieceType.ROOK, PieceType.KNIGHT, PieceType.BISHOP, PieceType.QUEEN,
                     PieceType.KING, PieceType.BISHOP, PieceType.KNIGHT, PieceType.ROOK]
        piece_classes = {
            PieceType.ROOK: Rook, PieceType.KNIGHT: Knight, PieceType.BISHOP: Bishop,
            PieceType.QUEEN: Queen, PieceType.KING: King,
        }
        for col, piece_type in enumerate(back_rank):
            self.cells[0][col] = piece_classes[piece_type](Color.WHITE)
            self.cells[7][col] = piece_classes[piece_type](Color.BLACK)
        for col in range(Board.SIZE):
            self.cells[1][col] = Pawn(Color.WHITE)
            self.cells[6][col] = Pawn(Color.BLACK)

    def is_valid_cell(self, position: Position) -> bool:
        row, col = position
        return 0 <= row < Board.SIZE and 0 <= col < Board.SIZE

    def get_piece(self, position: Position) -> Optional[Piece]:
        row, col = position
        return self.cells[row][col]

    def set_piece(self, position: Position, piece: Optional[Piece]) -> None:
        row, col = position
        self.cells[row][col] = piece

    def move_piece(self, source: Position, destination: Position) -> Optional[Piece]:
        captured = self.get_piece(destination)
        self.set_piece(destination, self.get_piece(source))
        self.set_piece(source, None)
        return captured

    def find_king(self, color: Color) -> Optional[Position]:
        for row in range(Board.SIZE):
            for col in range(Board.SIZE):
                piece = self.cells[row][col]
                if piece is not None and piece.type == PieceType.KING and piece.color == color:
                    return (row, col)
        return None

    def all_pieces(self, color: Color) -> List[Tuple[Position, Piece]]:
        result = []
        for row in range(Board.SIZE):
            for col in range(Board.SIZE):
                piece = self.cells[row][col]
                if piece is not None and piece.color == color:
                    result.append(((row, col), piece))
        return result

    def print_board(self) -> None:
        for row in range(Board.SIZE - 1, -1, -1):
            rank = [self.cells[row][col].symbol() if self.cells[row][col] else "." for col in range(Board.SIZE)]
            print(row + 1, " ".join(rank))
        print("  " + " ".join("abcdefgh"))


class Player:
    def __init__(self, name: str, color: Color):
        self.name = name
        self.color = color


def parse_square(square: str) -> Position:
    col = ord(square[0]) - ord("a")
    row = int(square[1]) - 1
    return (row, col)


class IllegalMoveError(Exception):
    pass


class Game:
    def __init__(self, white_name: str = "White", black_name: str = "Black"):
        self.board = Board()
        self.white = Player(white_name, Color.WHITE)
        self.black = Player(black_name, Color.BLACK)
        self.current_player = self.white
        self.status = GameStatus.ONGOING
        # Undo stack: last move played sits at the end, ready to be reversed.
        self.history: List[Move] = []

    def switch_turn(self) -> None:
        self.current_player = self.black if self.current_player is self.white else self.white

    def is_square_attacked(self, position: Position, by_color: Color) -> bool:
        for piece_position, piece in self.board.all_pieces(by_color):
            if position in piece.get_attacked_squares(piece_position, self.board):
                return True
        return False

    def is_check(self, color: Color) -> bool:
        king_position = self.board.find_king(color)
        if king_position is None:
            return False
        return self.is_square_attacked(king_position, color.opposite())

    def _has_legal_move(self, color: Color) -> bool:
        for source, piece in self.board.all_pieces(color):
            for destination in piece.get_possible_moves(source, self.board):
                captured = self.board.move_piece(source, destination)
                still_in_check = self.is_check(color)
                self.board.set_piece(source, piece)
                self.board.set_piece(destination, captured)
                if not still_in_check:
                    return True
        return False

    def is_checkmate(self, color: Color) -> bool:
        return self.is_check(color) and not self._has_legal_move(color)

    def is_stalemate(self, color: Color) -> bool:
        return not self.is_check(color) and not self._has_legal_move(color)

    def move(self, source: Position, destination: Position) -> Move:
        if self.status in (GameStatus.CHECKMATE, GameStatus.STALEMATE):
            raise IllegalMoveError("Game is already over")

        piece = self.board.get_piece(source)
        if piece is None or piece.color != self.current_player.color:
            raise IllegalMoveError("No movable piece of the current player at source")

        if destination not in piece.get_possible_moves(source, self.board):
            raise IllegalMoveError("Illegal move for this piece")

        captured = self.board.move_piece(source, destination)
        if self.is_check(self.current_player.color):
            # Undo the tentative move: it would leave (or keep) our own king in check.
            self.board.set_piece(source, piece)
            self.board.set_piece(destination, captured)
            raise IllegalMoveError("Move leaves own king in check")

        # The Move record keeps `piece` (pre-promotion) as piece_moved so
        # undo() can restore the pawn even after we replace the board
        # occupant with a queen below.
        performed_move = Move(source, destination, piece, captured)
        self.history.append(performed_move)

        if piece.type == PieceType.PAWN and destination[0] in (0, Board.SIZE - 1):
            self.board.set_piece(destination, Queen(piece.color))

        self.switch_turn()
        self._recompute_status()
        return performed_move

    def undo(self) -> None:
        """Pops the most recent move and restores the board exactly as it
        was before that move, including undoing promotions (piece_moved is
        always the pre-promotion piece) and restoring captures."""
        if not self.history:
            raise IllegalMoveError("No moves to undo")
        last_move = self.history.pop()
        self.board.set_piece(last_move.source, last_move.piece_moved)
        self.board.set_piece(last_move.destination, last_move.captured_piece)
        self.switch_turn()
        self._recompute_status()

    def _recompute_status(self) -> None:
        to_move = self.current_player.color
        if self.is_checkmate(to_move):
            self.status = GameStatus.CHECKMATE
        elif self.is_check(to_move):
            self.status = GameStatus.CHECK
        elif self.is_stalemate(to_move):
            self.status = GameStatus.STALEMATE
        else:
            self.status = GameStatus.ONGOING


def main() -> None:
    game = Game("Alice", "Bob")

    # Fool's Mate -- fastest possible checkmate, four half-moves.
    moves = ["f2f3", "e7e5", "g2g4", "d8h4"]
    for move_str in moves:
        source = parse_square(move_str[0:2])
        destination = parse_square(move_str[2:4])
        mover = game.current_player.name
        game.move(source, destination)
        print(f"{mover} played {move_str} -> status: {game.status.name}")

    print()
    game.board.print_board()
    print(f"\nStatus before undo: {game.status.name}")

    print("\n-- Undoing the checkmating move --")
    game.undo()
    print(f"Status after undo: {game.status.name} (back to move for {game.current_player.name})")
    game.board.print_board()


if __name__ == "__main__":
    main()
