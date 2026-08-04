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
    Every piece knows only its OWN movement rule (Piece::getPossibleMoves).
    Board/Game never special-case "if this is a bishop do X" -- they just
    call piece->getPossibleMoves(...) polymorphically. Adding a new piece
    type means adding one new subclass, nothing else changes.

Command-style Move objects for undo:
    Each executed Move records exactly what it takes to reverse itself: the
    piece that was AT the source square before the move (so promotion can
    be undone back to a pawn) and whatever piece occupied the destination
    square before it was captured (possibly nullptr). undo() pops the last
    Move and replays those two facts backwards -- it never needs to
    recompute anything from scratch.

Core Entities:
    Game             -- orchestrates turns, applies/undoes moves, tracks status
    Board            -- 8x8 grid of Piece references + board-level queries
    Piece (abstract)  -- Color, PieceType, getPossibleMoves() [King, Queen,
                         Rook, Bishop, Knight, Pawn]
    Move             -- source, destination, pieceMoved (pre-move), capturedPiece
    Player           -- name + color
    GameStatus       -- ONGOING, CHECK, CHECKMATE, STALEMATE

Out of scope: castling, en passant, draw by repetition/50-move rule.
================================================================================
*/

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using Position = std::pair<int, int>;  // (row, col); row 0 = rank 1, col 0 = file 'a'.

enum class Color { WHITE, BLACK };

Color opposite(Color color) { return color == Color::WHITE ? Color::BLACK : Color::WHITE; }

enum class PieceType { KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN };

enum class GameStatus { ONGOING, CHECK, CHECKMATE, STALEMATE };

class Board;

class Piece {
public:
    Piece(Color color, PieceType type) : color_(color), type_(type) {}
    virtual ~Piece() = default;

    // Pseudo-legal moves: obeys this piece's movement/blocking/capture
    // rules, but ignores whether the move leaves the mover's own king in
    // check -- Game::move() layers that safety check on top.
    virtual std::vector<Position> getPossibleMoves(Position position, const Board& board) const = 0;

    // Squares this piece threatens; used for check detection. Identical to
    // getPossibleMoves for every piece except the pawn, whose diagonal
    // attacks threaten a square whether or not an enemy is standing on it
    // yet (a plain move-list wouldn't include an empty diagonal square).
    virtual std::vector<Position> getAttackedSquares(Position position, const Board& board) const {
        return getPossibleMoves(position, board);
    }

    char symbol() const {
        char letter;
        switch (type_) {
            case PieceType::KING: letter = 'K'; break;
            case PieceType::QUEEN: letter = 'Q'; break;
            case PieceType::ROOK: letter = 'R'; break;
            case PieceType::BISHOP: letter = 'B'; break;
            case PieceType::KNIGHT: letter = 'N'; break;
            case PieceType::PAWN: letter = 'P'; break;
        }
        return color_ == Color::WHITE ? letter : static_cast<char>(std::tolower(letter));
    }

    Color color() const { return color_; }
    PieceType type() const { return type_; }

protected:
    // Shared helper for the sliding pieces (rook/bishop/queen): walk each
    // direction until the edge of the board, a friendly piece (stop before
    // it), or an enemy piece (stop after capturing it).
    std::vector<Position> slide(Position position, const Board& board,
                                 const std::vector<Position>& directions) const;

    Color color_;
    PieceType type_;
};

class Board {
public:
    static constexpr int SIZE = 8;

    Board() : cells_(SIZE, std::vector<std::shared_ptr<Piece>>(SIZE, nullptr)) {}

    void initialize();

    bool isValidCell(Position position) const {
        auto [row, col] = position;
        return row >= 0 && row < SIZE && col >= 0 && col < SIZE;
    }

    std::shared_ptr<Piece> getPiece(Position position) const {
        auto [row, col] = position;
        return cells_[row][col];
    }

    void setPiece(Position position, std::shared_ptr<Piece> piece) {
        auto [row, col] = position;
        cells_[row][col] = std::move(piece);
    }

    std::shared_ptr<Piece> movePiece(Position source, Position destination) {
        auto captured = getPiece(destination);
        setPiece(destination, getPiece(source));
        setPiece(source, nullptr);
        return captured;
    }

    std::optional<Position> findKing(Color color) const {
        for (int row = 0; row < SIZE; ++row) {
            for (int col = 0; col < SIZE; ++col) {
                auto piece = cells_[row][col];
                if (piece && piece->type() == PieceType::KING && piece->color() == color) {
                    return Position{row, col};
                }
            }
        }
        return std::nullopt;
    }

    std::vector<std::pair<Position, std::shared_ptr<Piece>>> allPieces(Color color) const {
        std::vector<std::pair<Position, std::shared_ptr<Piece>>> result;
        for (int row = 0; row < SIZE; ++row) {
            for (int col = 0; col < SIZE; ++col) {
                auto piece = cells_[row][col];
                if (piece && piece->color() == color) {
                    result.push_back({Position{row, col}, piece});
                }
            }
        }
        return result;
    }

    void printBoard() const {
        for (int row = SIZE - 1; row >= 0; --row) {
            std::cout << (row + 1) << " ";
            for (int col = 0; col < SIZE; ++col) {
                auto piece = cells_[row][col];
                std::cout << (piece ? piece->symbol() : '.') << " ";
            }
            std::cout << "\n";
        }
        std::cout << "  a b c d e f g h\n";
    }

private:
    std::vector<std::vector<std::shared_ptr<Piece>>> cells_;
};

std::vector<Position> Piece::slide(Position position, const Board& board,
                                    const std::vector<Position>& directions) const {
    std::vector<Position> moves;
    auto [row, col] = position;
    for (auto [dRow, dCol] : directions) {
        int r = row + dRow;
        int c = col + dCol;
        while (board.isValidCell({r, c})) {
            auto occupant = board.getPiece({r, c});
            if (!occupant) {
                moves.push_back({r, c});
            } else {
                if (occupant->color() != color_) {
                    moves.push_back({r, c});
                }
                break;
            }
            r += dRow;
            c += dCol;
        }
    }
    return moves;
}

class Rook : public Piece {
public:
    explicit Rook(Color color) : Piece(color, PieceType::ROOK) {}

    std::vector<Position> getPossibleMoves(Position position, const Board& board) const override {
        return slide(position, board, {{1, 0}, {-1, 0}, {0, 1}, {0, -1}});
    }
};

class Bishop : public Piece {
public:
    explicit Bishop(Color color) : Piece(color, PieceType::BISHOP) {}

    std::vector<Position> getPossibleMoves(Position position, const Board& board) const override {
        return slide(position, board, {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}});
    }
};

class Queen : public Piece {
public:
    explicit Queen(Color color) : Piece(color, PieceType::QUEEN) {}

    std::vector<Position> getPossibleMoves(Position position, const Board& board) const override {
        return slide(position, board,
                      {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}});
    }
};

class Knight : public Piece {
public:
    explicit Knight(Color color) : Piece(color, PieceType::KNIGHT) {}

    std::vector<Position> getPossibleMoves(Position position, const Board& board) const override {
        auto [row, col] = position;
        std::vector<Position> moves;
        static const std::vector<Position> offsets = {
            {2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2},
        };
        for (auto [dRow, dCol] : offsets) {
            Position candidate{row + dRow, col + dCol};
            if (board.isValidCell(candidate)) {
                auto occupant = board.getPiece(candidate);
                if (!occupant || occupant->color() != color_) {
                    moves.push_back(candidate);
                }
            }
        }
        return moves;
    }
};

class King : public Piece {
public:
    explicit King(Color color) : Piece(color, PieceType::KING) {}

    std::vector<Position> getPossibleMoves(Position position, const Board& board) const override {
        auto [row, col] = position;
        std::vector<Position> moves;
        for (int dRow = -1; dRow <= 1; ++dRow) {
            for (int dCol = -1; dCol <= 1; ++dCol) {
                if (dRow == 0 && dCol == 0) continue;
                Position candidate{row + dRow, col + dCol};
                if (board.isValidCell(candidate)) {
                    auto occupant = board.getPiece(candidate);
                    if (!occupant || occupant->color() != color_) {
                        moves.push_back(candidate);
                    }
                }
            }
        }
        return moves;
    }
};

class Pawn : public Piece {
public:
    explicit Pawn(Color color) : Piece(color, PieceType::PAWN) {}

    std::vector<Position> getPossibleMoves(Position position, const Board& board) const override {
        auto [row, col] = position;
        int direction = color_ == Color::WHITE ? 1 : -1;
        int startRow = color_ == Color::WHITE ? 1 : 6;
        std::vector<Position> moves;

        Position oneStep{row + direction, col};
        if (board.isValidCell(oneStep) && !board.getPiece(oneStep)) {
            moves.push_back(oneStep);
            Position twoStep{row + 2 * direction, col};
            if (row == startRow && !board.getPiece(twoStep)) {
                moves.push_back(twoStep);
            }
        }

        for (int dCol : {-1, 1}) {
            Position capture{row + direction, col + dCol};
            if (board.isValidCell(capture)) {
                auto occupant = board.getPiece(capture);
                if (occupant && occupant->color() != color_) {
                    moves.push_back(capture);
                }
            }
        }
        return moves;
    }

    std::vector<Position> getAttackedSquares(Position position, const Board& board) const override {
        auto [row, col] = position;
        int direction = color_ == Color::WHITE ? 1 : -1;
        std::vector<Position> squares;
        for (int dCol : {-1, 1}) {
            Position candidate{row + direction, col + dCol};
            if (board.isValidCell(candidate)) {
                squares.push_back(candidate);
            }
        }
        return squares;
    }
};

void Board::initialize() {
    static const std::vector<PieceType> backRank = {
        PieceType::ROOK, PieceType::KNIGHT, PieceType::BISHOP, PieceType::QUEEN,
        PieceType::KING, PieceType::BISHOP, PieceType::KNIGHT, PieceType::ROOK,
    };
    auto makePiece = [](PieceType type, Color color) -> std::shared_ptr<Piece> {
        switch (type) {
            case PieceType::ROOK: return std::make_shared<Rook>(color);
            case PieceType::KNIGHT: return std::make_shared<Knight>(color);
            case PieceType::BISHOP: return std::make_shared<Bishop>(color);
            case PieceType::QUEEN: return std::make_shared<Queen>(color);
            case PieceType::KING: return std::make_shared<King>(color);
            default: throw std::invalid_argument("Unsupported back-rank piece type");
        }
    };
    for (int col = 0; col < SIZE; ++col) {
        cells_[0][col] = makePiece(backRank[col], Color::WHITE);
        cells_[7][col] = makePiece(backRank[col], Color::BLACK);
    }
    for (int col = 0; col < SIZE; ++col) {
        cells_[1][col] = std::make_shared<Pawn>(Color::WHITE);
        cells_[6][col] = std::make_shared<Pawn>(Color::BLACK);
    }
}

// A Move is a self-contained "command": it records the two facts needed to
// reverse itself (pieceMoved -- as it was BEFORE this move, so a promoted
// queen can be undone back into a pawn -- and capturedPiece, possibly
// nullptr), so Game::undo() never has to recompute board state from scratch.
struct Move {
    Position source;
    Position destination;
    std::shared_ptr<Piece> pieceMoved;
    std::shared_ptr<Piece> capturedPiece;
};

struct Player {
    std::string name;
    Color color;
};

Position parseSquare(const std::string& square) {
    int col = square[0] - 'a';
    int row = square[1] - '1';
    return {row, col};
}

class IllegalMoveError : public std::runtime_error {
public:
    explicit IllegalMoveError(const std::string& message) : std::runtime_error(message) {}
};

class Game {
public:
    Game(std::string whiteName = "White", std::string blackName = "Black")
        : white_{std::move(whiteName), Color::WHITE},
          black_{std::move(blackName), Color::BLACK},
          currentPlayer_(&white_),
          status_(GameStatus::ONGOING) {
        board_.initialize();
    }

    void switchTurn() { currentPlayer_ = currentPlayer_ == &white_ ? &black_ : &white_; }

    bool isSquareAttacked(Position position, Color byColor) const {
        for (auto& [piecePosition, piece] : board_.allPieces(byColor)) {
            auto attacked = piece->getAttackedSquares(piecePosition, board_);
            if (std::find(attacked.begin(), attacked.end(), position) != attacked.end()) {
                return true;
            }
        }
        return false;
    }

    bool isCheck(Color color) const {
        auto kingPosition = board_.findKing(color);
        if (!kingPosition) return false;
        return isSquareAttacked(*kingPosition, opposite(color));
    }

    bool isCheckmate(Color color) { return isCheck(color) && !hasLegalMove(color); }

    bool isStalemate(Color color) { return !isCheck(color) && !hasLegalMove(color); }

    Move move(Position source, Position destination) {
        if (status_ == GameStatus::CHECKMATE || status_ == GameStatus::STALEMATE) {
            throw IllegalMoveError("Game is already over");
        }

        auto piece = board_.getPiece(source);
        if (!piece || piece->color() != currentPlayer_->color) {
            throw IllegalMoveError("No movable piece of the current player at source");
        }

        auto possible = piece->getPossibleMoves(source, board_);
        if (std::find(possible.begin(), possible.end(), destination) == possible.end()) {
            throw IllegalMoveError("Illegal move for this piece");
        }

        auto captured = board_.movePiece(source, destination);
        if (isCheck(currentPlayer_->color)) {
            // Undo the tentative move: it would leave (or keep) our own king in check.
            board_.setPiece(source, piece);
            board_.setPiece(destination, captured);
            throw IllegalMoveError("Move leaves own king in check");
        }

        // The Move record keeps `piece` (pre-promotion) as pieceMoved so
        // undo() can restore the pawn even after we replace the board
        // occupant with a queen below.
        Move performedMove{source, destination, piece, captured};
        history_.push_back(performedMove);

        if (piece->type() == PieceType::PAWN && (destination.first == 0 || destination.first == Board::SIZE - 1)) {
            board_.setPiece(destination, std::make_shared<Queen>(piece->color()));
        }

        switchTurn();
        recomputeStatus();
        return performedMove;
    }

    // Pops the most recent move and restores the board exactly as it was
    // before that move, including undoing promotions (pieceMoved is always
    // the pre-promotion piece) and restoring captures.
    void undo() {
        if (history_.empty()) {
            throw std::logic_error("No moves to undo");
        }
        Move lastMove = history_.back();
        history_.pop_back();
        board_.setPiece(lastMove.source, lastMove.pieceMoved);
        board_.setPiece(lastMove.destination, lastMove.capturedPiece);
        switchTurn();
        recomputeStatus();
    }

    const Player& currentPlayer() const { return *currentPlayer_; }
    GameStatus status() const { return status_; }
    Board& board() { return board_; }

    static std::string statusName(GameStatus status) {
        switch (status) {
            case GameStatus::ONGOING: return "ONGOING";
            case GameStatus::CHECK: return "CHECK";
            case GameStatus::CHECKMATE: return "CHECKMATE";
            case GameStatus::STALEMATE: return "STALEMATE";
        }
        return "UNKNOWN";
    }

private:
    bool hasLegalMove(Color color) {
        for (auto& [source, piece] : board_.allPieces(color)) {
            for (auto destination : piece->getPossibleMoves(source, board_)) {
                auto captured = board_.movePiece(source, destination);
                bool stillInCheck = isCheck(color);
                board_.setPiece(source, piece);
                board_.setPiece(destination, captured);
                if (!stillInCheck) return true;
            }
        }
        return false;
    }

    void recomputeStatus() {
        Color toMove = currentPlayer_->color;
        if (isCheckmate(toMove)) {
            status_ = GameStatus::CHECKMATE;
        } else if (isCheck(toMove)) {
            status_ = GameStatus::CHECK;
        } else if (isStalemate(toMove)) {
            status_ = GameStatus::STALEMATE;
        } else {
            status_ = GameStatus::ONGOING;
        }
    }

    Board board_;
    Player white_;
    Player black_;
    Player* currentPlayer_;
    GameStatus status_;
    // Undo stack: last move played sits at the back, ready to be reversed.
    std::vector<Move> history_;
};

int main() {
    Game game("Alice", "Bob");

    // Fool's Mate -- fastest possible checkmate, four half-moves.
    std::vector<std::string> moves = {"f2f3", "e7e5", "g2g4", "d8h4"};
    for (const auto& moveStr : moves) {
        Position source = parseSquare(moveStr.substr(0, 2));
        Position destination = parseSquare(moveStr.substr(2, 2));
        std::string mover = game.currentPlayer().name;
        game.move(source, destination);
        std::cout << mover << " played " << moveStr << " -> status: " << Game::statusName(game.status()) << "\n";
    }

    std::cout << "\n";
    game.board().printBoard();
    std::cout << "\nStatus before undo: " << Game::statusName(game.status()) << "\n";

    std::cout << "\n-- Undoing the checkmating move --\n";
    game.undo();
    std::cout << "Status after undo: " << Game::statusName(game.status())
               << " (back to move for " << game.currentPlayer().name << ")\n";
    game.board().printBoard();

    return 0;
}
