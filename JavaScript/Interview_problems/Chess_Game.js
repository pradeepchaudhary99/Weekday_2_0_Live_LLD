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
       castling) should not require touching unrelated piece classes.

--------------------------------------------------------------------------
Design
--------------------------------------------------------------------------
Strategy pattern for movement:
    Every piece knows only its OWN movement rule (Piece#getPossibleMoves).
    Board/Game never special-case "if this is a bishop do X" -- they just
    call piece.getPossibleMoves(...) polymorphically. Adding a new piece
    type means adding one new subclass, nothing else changes.

Command-style Move objects for undo:
    Each executed Move records exactly what it takes to reverse itself: the
    piece that was AT the source square before the move (so promotion can
    be undone back to a pawn) and whatever piece occupied the destination
    square before it was captured (possibly null). undo() pops the last
    Move and replays those two facts backwards -- it never needs to
    recompute anything from scratch.

Core Entities:
    Game             -- orchestrates turns, applies/undoes moves, tracks status
    Board            -- 8x8 grid of Piece references + board-level queries
    Piece (abstract)  -- color, type, getPossibleMoves() [King, Queen,
                         Rook, Bishop, Knight, Pawn]
    Move             -- source, destination, pieceMoved (pre-move), capturedPiece
    Player           -- name + color
    GameStatus       -- ONGOING, CHECK, CHECKMATE, STALEMATE

Out of scope: castling, en passant, draw by repetition/50-move rule.
================================================================================
*/

'use strict';

const Color = Object.freeze({ WHITE: 'WHITE', BLACK: 'BLACK' });

function opposite(color) {
    return color === Color.WHITE ? Color.BLACK : Color.WHITE;
}

const PieceType = Object.freeze({
    KING: 'KING', QUEEN: 'QUEEN', ROOK: 'ROOK', BISHOP: 'BISHOP', KNIGHT: 'KNIGHT', PAWN: 'PAWN',
});

const GameStatus = Object.freeze({
    ONGOING: 'ONGOING', CHECK: 'CHECK', CHECKMATE: 'CHECKMATE', STALEMATE: 'STALEMATE',
});

// A Position is just a {row, col} pair; row 0 = rank 1 (White's back rank),
// col 0 = file 'a'. Two positions are compared by value with samePosition
// since plain object identity would never match across calls.
function samePosition(a, b) {
    return a.row === b.row && a.col === b.col;
}

class Piece {
    constructor(color, type) {
        this.color = color;
        this.type = type;
    }

    // Pseudo-legal moves: obeys this piece's movement/blocking/capture
    // rules, but ignores whether the move leaves the mover's own king in
    // check -- Game.move() layers that safety check on top.
    // eslint-disable-next-line no-unused-vars
    getPossibleMoves(position, board) {
        throw new Error('getPossibleMoves must be implemented by subclass');
    }

    // Squares this piece threatens; used for check detection. Identical to
    // getPossibleMoves for every piece except the pawn, whose diagonal
    // attacks threaten a square whether or not an enemy is standing on it
    // yet (a plain move-list wouldn't include an empty diagonal square).
    getAttackedSquares(position, board) {
        return this.getPossibleMoves(position, board);
    }

    symbol() {
        const letters = {
            KING: 'K', QUEEN: 'Q', ROOK: 'R', BISHOP: 'B', KNIGHT: 'N', PAWN: 'P',
        };
        const letter = letters[this.type];
        return this.color === Color.WHITE ? letter : letter.toLowerCase();
    }

    // Shared helper for the sliding pieces (rook/bishop/queen): walk each
    // direction until the edge of the board, a friendly piece (stop before
    // it), or an enemy piece (stop after capturing it).
    _slide(position, board, directions) {
        const moves = [];
        for (const [dRow, dCol] of directions) {
            let r = position.row + dRow;
            let c = position.col + dCol;
            while (board.isValidCell(r, c)) {
                const occupant = board.getPiece(r, c);
                if (!occupant) {
                    moves.push({ row: r, col: c });
                } else {
                    if (occupant.color !== this.color) {
                        moves.push({ row: r, col: c });
                    }
                    break;
                }
                r += dRow;
                c += dCol;
            }
        }
        return moves;
    }
}

class Rook extends Piece {
    constructor(color) {
        super(color, PieceType.ROOK);
    }

    getPossibleMoves(position, board) {
        return this._slide(position, board, [[1, 0], [-1, 0], [0, 1], [0, -1]]);
    }
}

class Bishop extends Piece {
    constructor(color) {
        super(color, PieceType.BISHOP);
    }

    getPossibleMoves(position, board) {
        return this._slide(position, board, [[1, 1], [1, -1], [-1, 1], [-1, -1]]);
    }
}

class Queen extends Piece {
    constructor(color) {
        super(color, PieceType.QUEEN);
    }

    getPossibleMoves(position, board) {
        return this._slide(position, board, [
            [1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1],
        ]);
    }
}

const KNIGHT_OFFSETS = [[2, 1], [2, -1], [-2, 1], [-2, -1], [1, 2], [1, -2], [-1, 2], [-1, -2]];

class Knight extends Piece {
    constructor(color) {
        super(color, PieceType.KNIGHT);
    }

    getPossibleMoves(position, board) {
        const moves = [];
        for (const [dRow, dCol] of KNIGHT_OFFSETS) {
            const r = position.row + dRow;
            const c = position.col + dCol;
            if (board.isValidCell(r, c)) {
                const occupant = board.getPiece(r, c);
                if (!occupant || occupant.color !== this.color) {
                    moves.push({ row: r, col: c });
                }
            }
        }
        return moves;
    }
}

class King extends Piece {
    constructor(color) {
        super(color, PieceType.KING);
    }

    getPossibleMoves(position, board) {
        const moves = [];
        for (let dRow = -1; dRow <= 1; dRow++) {
            for (let dCol = -1; dCol <= 1; dCol++) {
                if (dRow === 0 && dCol === 0) continue;
                const r = position.row + dRow;
                const c = position.col + dCol;
                if (board.isValidCell(r, c)) {
                    const occupant = board.getPiece(r, c);
                    if (!occupant || occupant.color !== this.color) {
                        moves.push({ row: r, col: c });
                    }
                }
            }
        }
        return moves;
    }
}

class Pawn extends Piece {
    constructor(color) {
        super(color, PieceType.PAWN);
    }

    getPossibleMoves(position, board) {
        const direction = this.color === Color.WHITE ? 1 : -1;
        const startRow = this.color === Color.WHITE ? 1 : 6;
        const moves = [];

        const oneStep = { row: position.row + direction, col: position.col };
        if (board.isValidCell(oneStep.row, oneStep.col) && !board.getPiece(oneStep.row, oneStep.col)) {
            moves.push(oneStep);
            const twoStep = { row: position.row + 2 * direction, col: position.col };
            if (position.row === startRow && !board.getPiece(twoStep.row, twoStep.col)) {
                moves.push(twoStep);
            }
        }

        for (const dCol of [-1, 1]) {
            const r = position.row + direction;
            const c = position.col + dCol;
            if (board.isValidCell(r, c)) {
                const occupant = board.getPiece(r, c);
                if (occupant && occupant.color !== this.color) {
                    moves.push({ row: r, col: c });
                }
            }
        }
        return moves;
    }

    getAttackedSquares(position, board) {
        const direction = this.color === Color.WHITE ? 1 : -1;
        const squares = [];
        for (const dCol of [-1, 1]) {
            const r = position.row + direction;
            const c = position.col + dCol;
            if (board.isValidCell(r, c)) {
                squares.push({ row: r, col: c });
            }
        }
        return squares;
    }
}

// A Move is a self-contained "command": it records the two facts needed to
// reverse itself (pieceMoved -- as it was BEFORE this move, so a promoted
// queen can be undone back into a pawn -- and capturedPiece, possibly
// null), so Game#undo never has to recompute board state from scratch.
class Move {
    constructor(source, destination, pieceMoved, capturedPiece) {
        this.source = source;
        this.destination = destination;
        this.pieceMoved = pieceMoved;
        this.capturedPiece = capturedPiece;
    }
}

class Board {
    static SIZE = 8;

    constructor() {
        this.cells = Array.from({ length: Board.SIZE }, () => new Array(Board.SIZE).fill(null));
        this.initialize();
    }

    initialize() {
        const backRank = [
            PieceType.ROOK, PieceType.KNIGHT, PieceType.BISHOP, PieceType.QUEEN,
            PieceType.KING, PieceType.BISHOP, PieceType.KNIGHT, PieceType.ROOK,
        ];
        for (let col = 0; col < Board.SIZE; col++) {
            this.cells[0][col] = Board._makePiece(backRank[col], Color.WHITE);
            this.cells[7][col] = Board._makePiece(backRank[col], Color.BLACK);
        }
        for (let col = 0; col < Board.SIZE; col++) {
            this.cells[1][col] = new Pawn(Color.WHITE);
            this.cells[6][col] = new Pawn(Color.BLACK);
        }
    }

    static _makePiece(type, color) {
        switch (type) {
            case PieceType.ROOK: return new Rook(color);
            case PieceType.KNIGHT: return new Knight(color);
            case PieceType.BISHOP: return new Bishop(color);
            case PieceType.QUEEN: return new Queen(color);
            case PieceType.KING: return new King(color);
            default: throw new Error('Unsupported back-rank piece type');
        }
    }

    isValidCell(row, col) {
        return row >= 0 && row < Board.SIZE && col >= 0 && col < Board.SIZE;
    }

    getPiece(row, col) {
        return this.cells[row][col];
    }

    setPiece(position, piece) {
        this.cells[position.row][position.col] = piece;
    }

    movePiece(source, destination) {
        const captured = this.getPiece(destination.row, destination.col);
        this.setPiece(destination, this.getPiece(source.row, source.col));
        this.setPiece(source, null);
        return captured;
    }

    findKing(color) {
        for (let row = 0; row < Board.SIZE; row++) {
            for (let col = 0; col < Board.SIZE; col++) {
                const piece = this.cells[row][col];
                if (piece && piece.type === PieceType.KING && piece.color === color) {
                    return { row, col };
                }
            }
        }
        return null;
    }

    allPieces(color) {
        const result = [];
        for (let row = 0; row < Board.SIZE; row++) {
            for (let col = 0; col < Board.SIZE; col++) {
                const piece = this.cells[row][col];
                if (piece && piece.color === color) {
                    result.push({ position: { row, col }, piece });
                }
            }
        }
        return result;
    }

    printBoard() {
        for (let row = Board.SIZE - 1; row >= 0; row--) {
            let line = `${row + 1} `;
            for (let col = 0; col < Board.SIZE; col++) {
                const piece = this.cells[row][col];
                line += piece ? piece.symbol() : '.';
                if (col !== Board.SIZE - 1) line += ' ';
            }
            console.log(line);
        }
        console.log('  a b c d e f g h');
    }
}

class Player {
    constructor(name, color) {
        this.name = name;
        this.color = color;
    }
}

function parseSquare(square) {
    const col = square.charCodeAt(0) - 'a'.charCodeAt(0);
    const row = parseInt(square[1], 10) - 1;
    return { row, col };
}

class IllegalMoveError extends Error {}

class Game {
    constructor(whiteName = 'White', blackName = 'Black') {
        this.board = new Board();
        this.white = new Player(whiteName, Color.WHITE);
        this.black = new Player(blackName, Color.BLACK);
        this.currentPlayer = this.white;
        this.status = GameStatus.ONGOING;
        // Undo stack: last move played sits at the end, ready to be reversed.
        this.history = [];
    }

    switchTurn() {
        this.currentPlayer = this.currentPlayer === this.white ? this.black : this.white;
    }

    isSquareAttacked(position, byColor) {
        for (const { position: piecePosition, piece } of this.board.allPieces(byColor)) {
            const attacked = piece.getAttackedSquares(piecePosition, this.board);
            if (attacked.some((square) => samePosition(square, position))) {
                return true;
            }
        }
        return false;
    }

    isCheck(color) {
        const kingPosition = this.board.findKing(color);
        if (!kingPosition) return false;
        return this.isSquareAttacked(kingPosition, opposite(color));
    }

    _hasLegalMove(color) {
        for (const { position: source, piece } of this.board.allPieces(color)) {
            for (const destination of piece.getPossibleMoves(source, this.board)) {
                const captured = this.board.movePiece(source, destination);
                const stillInCheck = this.isCheck(color);
                this.board.setPiece(source, piece);
                this.board.setPiece(destination, captured);
                if (!stillInCheck) return true;
            }
        }
        return false;
    }

    isCheckmate(color) {
        return this.isCheck(color) && !this._hasLegalMove(color);
    }

    isStalemate(color) {
        return !this.isCheck(color) && !this._hasLegalMove(color);
    }

    move(source, destination) {
        if (this.status === GameStatus.CHECKMATE || this.status === GameStatus.STALEMATE) {
            throw new IllegalMoveError('Game is already over');
        }

        const piece = this.board.getPiece(source.row, source.col);
        if (!piece || piece.color !== this.currentPlayer.color) {
            throw new IllegalMoveError('No movable piece of the current player at source');
        }

        const possible = piece.getPossibleMoves(source, this.board);
        if (!possible.some((square) => samePosition(square, destination))) {
            throw new IllegalMoveError('Illegal move for this piece');
        }

        let captured = this.board.movePiece(source, destination);
        if (this.isCheck(this.currentPlayer.color)) {
            // Undo the tentative move: it would leave (or keep) our own king in check.
            this.board.setPiece(source, piece);
            this.board.setPiece(destination, captured);
            throw new IllegalMoveError('Move leaves own king in check');
        }

        // The Move record keeps `piece` (pre-promotion) as pieceMoved so
        // undo() can restore the pawn even after we replace the board
        // occupant with a queen below.
        const performedMove = new Move(source, destination, piece, captured);
        this.history.push(performedMove);

        if (piece.type === PieceType.PAWN && (destination.row === 0 || destination.row === Board.SIZE - 1)) {
            this.board.setPiece(destination, new Queen(piece.color));
        }

        this.switchTurn();
        this._recomputeStatus();
        return performedMove;
    }

    // Pops the most recent move and restores the board exactly as it was
    // before that move, including undoing promotions (pieceMoved is always
    // the pre-promotion piece) and restoring captures.
    undo() {
        if (this.history.length === 0) {
            throw new IllegalMoveError('No moves to undo');
        }
        const lastMove = this.history.pop();
        this.board.setPiece(lastMove.source, lastMove.pieceMoved);
        this.board.setPiece(lastMove.destination, lastMove.capturedPiece);
        this.switchTurn();
        this._recomputeStatus();
    }

    _recomputeStatus() {
        const toMove = this.currentPlayer.color;
        if (this.isCheckmate(toMove)) {
            this.status = GameStatus.CHECKMATE;
        } else if (this.isCheck(toMove)) {
            this.status = GameStatus.CHECK;
        } else if (this.isStalemate(toMove)) {
            this.status = GameStatus.STALEMATE;
        } else {
            this.status = GameStatus.ONGOING;
        }
    }
}

function main() {
    const game = new Game('Alice', 'Bob');

    // Fool's Mate -- fastest possible checkmate, four half-moves.
    const moves = ['f2f3', 'e7e5', 'g2g4', 'd8h4'];
    for (const moveStr of moves) {
        const source = parseSquare(moveStr.slice(0, 2));
        const destination = parseSquare(moveStr.slice(2, 4));
        const mover = game.currentPlayer.name;
        game.move(source, destination);
        console.log(`${mover} played ${moveStr} -> status: ${game.status}`);
    }

    console.log();
    game.board.printBoard();
    console.log(`\nStatus before undo: ${game.status}`);

    console.log('\n-- Undoing the checkmating move --');
    game.undo();
    console.log(`Status after undo: ${game.status} (back to move for ${game.currentPlayer.name})`);
    game.board.printBoard();
}

if (require.main === module) {
    main();
}

module.exports = {
    Color, PieceType, GameStatus, Piece, Rook, Bishop, Queen, Knight, King, Pawn,
    Move, Board, Player, Game, IllegalMoveError, parseSquare,
};
