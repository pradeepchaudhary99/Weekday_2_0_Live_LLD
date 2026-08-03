/*
Functional Requirements:
    Support a standard 8x8
    Enforce peice-specific movements and capture rules(KING, QUEEN, ROOK, BISHOP, Knight, PAWN)
    Detect check, checkmate, and stalemate after every move.
    Maintain move history to support undo

Non-Functional Requirements:

Move validation latency

*/

// Game
// Board
// Square
// Piece(Abstract) : Color, CurrentPosition, MovementStrategy
// KING, QUEEN, ROOK, BISHOP, Knight, PAWN
//
// MovementStrategy
//     --> Every Piece will have its logic for movement strategy
//     --> Validation logic for every piece
//
// Player
// GameState
// CheckDetector

class ChessGame {
}

module.exports = { ChessGame };
