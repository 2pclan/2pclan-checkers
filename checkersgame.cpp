#include "checkersgame.h"
#include <QDataStream>
#include <QIODevice>

CheckersGame::CheckersGame(QObject *parent)
    : QObject(parent)
    , m_currentPlayer(PlayerColor::Red)
    , m_winner(PlayerColor::None)
{
    resetGame();
}

void CheckersGame::resetGame()
{
    m_currentPlayer = PlayerColor::Red; // Red goes first
    m_winner = PlayerColor::None;
    initializeBoard();
    emit boardChanged();
    emit turnChanged(m_currentPlayer);
}

void CheckersGame::initializeBoard()
{
    // Clear board
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            m_board[row][col] = Piece::Empty;
        }
    }
    
    // Place black pieces (top 3 rows)
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            // Only on dark squares (where row + col is odd)
            if ((row + col) % 2 == 1) {
                m_board[row][col] = Piece::Black;
            }
        }
    }
    
    // Place red pieces (bottom 3 rows)
    for (int row = 5; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            // Only on dark squares
            if ((row + col) % 2 == 1) {
                m_board[row][col] = Piece::Red;
            }
        }
    }
}

Piece CheckersGame::pieceAt(int row, int col) const
{
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return Piece::Empty;
    }
    return m_board[row][col];
}

Piece CheckersGame::pieceAt(const QPoint& pos) const
{
    return pieceAt(pos.y(), pos.x());
}

PlayerColor CheckersGame::pieceOwner(Piece piece)
{
    switch (piece) {
        case Piece::Red:
        case Piece::RedKing:
            return PlayerColor::Red;
        case Piece::Black:
        case Piece::BlackKing:
            return PlayerColor::Black;
        default:
            return PlayerColor::None;
    }
}

bool CheckersGame::isKing(Piece piece)
{
    return piece == Piece::RedKing || piece == Piece::BlackKing;
}

bool CheckersGame::isPlayerPiece(const QPoint& pos, PlayerColor player) const
{
    return pieceOwner(pieceAt(pos)) == player;
}

bool CheckersGame::isValidPosition(const QPoint& pos) const
{
    return pos.x() >= 0 && pos.x() < BOARD_SIZE && 
           pos.y() >= 0 && pos.y() < BOARD_SIZE;
}

bool CheckersGame::isEmpty(const QPoint& pos) const
{
    return isValidPosition(pos) && pieceAt(pos) == Piece::Empty;
}

bool CheckersGame::isOpponent(const QPoint& pos, PlayerColor player) const
{
    if (!isValidPosition(pos)) return false;
    PlayerColor owner = pieceOwner(pieceAt(pos));
    return owner != PlayerColor::None && owner != player;
}

QVector<QPoint> CheckersGame::getAllMovablePieces(PlayerColor player) const
{
    QVector<QPoint> movable;
    bool mustCapture = playerHasCapture(player);
    
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            QPoint pos(col, row);
            if (isPlayerPiece(pos, player)) {
                QVector<Move> moves = getValidMoves(pos);
                if (!moves.isEmpty()) {
                    // If there's a capture available, only show pieces that can capture
                    if (mustCapture) {
                        for (const Move& m : moves) {
                            if (!m.captures.isEmpty()) {
                                movable.append(pos);
                                break;
                            }
                        }
                    } else {
                        movable.append(pos);
                    }
                }
            }
        }
    }
    
    return movable;
}

bool CheckersGame::playerHasCapture(PlayerColor player) const
{
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            QPoint pos(col, row);
            if (isPlayerPiece(pos, player) && canCapture(pos, player)) {
                return true;
            }
        }
    }
    return false;
}

bool CheckersGame::canCapture(const QPoint& from, PlayerColor player) const
{
    Piece piece = pieceAt(from);
    bool isK = isKing(piece);
    
    // Direction vectors for diagonal moves
    QVector<QPoint> directions;
    
    if (player == PlayerColor::Red || isK) {
        directions.append(QPoint(-1, -1)); // Up-left
        directions.append(QPoint(1, -1));  // Up-right
    }
    if (player == PlayerColor::Black || isK) {
        directions.append(QPoint(-1, 1));  // Down-left
        directions.append(QPoint(1, 1));   // Down-right
    }
    
    for (const QPoint& dir : directions) {
        QPoint mid = from + dir;
        QPoint to = from + dir * 2;
        
        if (isOpponent(mid, player) && isEmpty(to)) {
            return true;
        }
    }
    
    return false;
}

QVector<Move> CheckersGame::getValidMoves(const QPoint& from) const
{
    if (!isValidPosition(from)) return {};
    
    Piece piece = pieceAt(from);
    PlayerColor owner = pieceOwner(piece);
    
    if (owner == PlayerColor::None) return {};
    
    // If the current player has any capture available, they must capture
    bool mustCapture = playerHasCapture(owner);
    
    QVector<Move> captures = getCaptureMoves(from);
    
    if (mustCapture) {
        return captures; // Must return only capture moves
    }
    
    if (!captures.isEmpty()) {
        return captures; // Prefer captures
    }
    
    return getSimpleMoves(from);
}

QVector<Move> CheckersGame::getSimpleMoves(const QPoint& from) const
{
    QVector<Move> moves;
    Piece piece = pieceAt(from);
    PlayerColor owner = pieceOwner(piece);
    bool isK = isKing(piece);
    
    QVector<QPoint> directions;
    
    // Red moves up (negative y), Black moves down (positive y)
    if (owner == PlayerColor::Red || isK) {
        directions.append(QPoint(-1, -1));
        directions.append(QPoint(1, -1));
    }
    if (owner == PlayerColor::Black || isK) {
        directions.append(QPoint(-1, 1));
        directions.append(QPoint(1, 1));
    }
    
    for (const QPoint& dir : directions) {
        QPoint to = from + dir;
        if (isEmpty(to)) {
            moves.append({from, to, {}});
        }
    }
    
    return moves;
}

QVector<Move> CheckersGame::getCaptureMoves(const QPoint& from) const
{
    QVector<Move> moves;
    Piece piece = pieceAt(from);
    
    if (piece == Piece::Empty) return moves;
    
    // Create a temporary board for multi-jump calculations
    Piece tempBoard[BOARD_SIZE][BOARD_SIZE];
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            tempBoard[r][c] = m_board[r][c];
        }
    }
    
    QVector<QPoint> captured;
    findMultiJumps(from, from, piece, captured, moves, tempBoard);
    
    return moves;
}

void CheckersGame::findMultiJumps(const QPoint& current, const QPoint& original,
                                   Piece piece, QVector<QPoint>& captured,
                                   QVector<Move>& moves, 
                                   Piece tempBoard[BOARD_SIZE][BOARD_SIZE]) const
{
    PlayerColor owner = pieceOwner(piece);
    bool isK = isKing(piece);
    
    QVector<QPoint> directions;
    
    if (owner == PlayerColor::Red || isK) {
        directions.append(QPoint(-1, -1));
        directions.append(QPoint(1, -1));
    }
    if (owner == PlayerColor::Black || isK) {
        directions.append(QPoint(-1, 1));
        directions.append(QPoint(1, 1));
    }
    
    bool foundJump = false;
    
    for (const QPoint& dir : directions) {
        QPoint mid = current + dir;
        QPoint to = current + dir * 2;
        
        if (!isValidPosition(to)) continue;
        
        Piece midPiece = tempBoard[mid.y()][mid.x()];
        Piece toPiece = tempBoard[to.y()][to.x()];
        
        // Check if we can jump over an opponent piece to an empty square
        if (pieceOwner(midPiece) != PlayerColor::None && 
            pieceOwner(midPiece) != owner && 
            toPiece == Piece::Empty) {
            
            foundJump = true;
            
            // Temporarily make the jump
            tempBoard[current.y()][current.x()] = Piece::Empty;
            tempBoard[mid.y()][mid.x()] = Piece::Empty;
            tempBoard[to.y()][to.x()] = piece;
            
            captured.append(mid);
            
            // Recursively find more jumps
            findMultiJumps(to, original, piece, captured, moves, tempBoard);
            
            // Undo the jump
            captured.removeLast();
            tempBoard[current.y()][current.x()] = piece;
            tempBoard[mid.y()][mid.x()] = midPiece;
            tempBoard[to.y()][to.x()] = Piece::Empty;
        }
    }
    
    // If no more jumps found and we've made at least one capture, record the move
    if (!foundJump && !captured.isEmpty()) {
        moves.append({original, current, captured});
    }
}

bool CheckersGame::isValidMove(const Move& move) const
{
    if (!move.isValid()) return false;
    
    QVector<Move> validMoves = getValidMoves(move.from);
    
    for (const Move& valid : validMoves) {
        if (valid.from == move.from && valid.to == move.to) {
            return true;
        }
    }
    
    return false;
}

bool CheckersGame::makeMove(const Move& move)
{
    if (!isValidMove(move)) return false;
    
    // Find the full move with captures
    QVector<Move> validMoves = getValidMoves(move.from);
    Move fullMove = Move::invalid();
    
    for (const Move& valid : validMoves) {
        if (valid.from == move.from && valid.to == move.to) {
            fullMove = valid;
            break;
        }
    }
    
    if (!fullMove.isValid()) return false;
    
    Piece piece = pieceAt(fullMove.from);
    
    // Move the piece
    m_board[fullMove.from.y()][fullMove.from.x()] = Piece::Empty;
    m_board[fullMove.to.y()][fullMove.to.x()] = piece;
    
    // Remove captured pieces
    if (!fullMove.captures.isEmpty()) {
        for (const QPoint& cap : fullMove.captures) {
            m_board[cap.y()][cap.x()] = Piece::Empty;
        }
        emit piecesCaptured(fullMove.captures);
    }
    
    // Check for king promotion
    bool crowned = false;
    if (piece == Piece::Red && fullMove.to.y() == 0) {
        m_board[fullMove.to.y()][fullMove.to.x()] = Piece::RedKing;
        crowned = true;
    } else if (piece == Piece::Black && fullMove.to.y() == BOARD_SIZE - 1) {
        m_board[fullMove.to.y()][fullMove.to.x()] = Piece::BlackKing;
        crowned = true;
    }
    
    if (crowned) {
        emit pieceCrowned(fullMove.to);
    }
    
    emit boardChanged();
    
    // Switch turns
    switchPlayer();
    checkForWinner();
    
    return true;
}

void CheckersGame::switchPlayer()
{
    m_currentPlayer = (m_currentPlayer == PlayerColor::Red) 
                      ? PlayerColor::Black 
                      : PlayerColor::Red;
    emit turnChanged(m_currentPlayer);
}

void CheckersGame::checkForWinner()
{
    // Check if current player has any valid moves
    QVector<QPoint> movable = getAllMovablePieces(m_currentPlayer);
    
    if (movable.isEmpty()) {
        // Current player can't move - they lose
        m_winner = (m_currentPlayer == PlayerColor::Red) 
                   ? PlayerColor::Black 
                   : PlayerColor::Red;
        emit gameOver(m_winner);
    }
}

QByteArray CheckersGame::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    
    // Write board state
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            stream << static_cast<int>(m_board[row][col]);
        }
    }
    
    // Write game state
    stream << static_cast<int>(m_currentPlayer);
    stream << static_cast<int>(m_winner);
    
    return data;
}

void CheckersGame::deserialize(const QByteArray& data)
{
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_5_15);
    
    // Read board state
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            int piece;
            stream >> piece;
            m_board[row][col] = static_cast<Piece>(piece);
        }
    }
    
    // Read game state
    int currentPlayer, winner;
    stream >> currentPlayer >> winner;
    m_currentPlayer = static_cast<PlayerColor>(currentPlayer);
    m_winner = static_cast<PlayerColor>(winner);
    
    emit boardChanged();
    emit turnChanged(m_currentPlayer);
    
    if (m_winner != PlayerColor::None) {
        emit gameOver(m_winner);
    }
}
