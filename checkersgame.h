#ifndef CHECKERSGAME_H
#define CHECKERSGAME_H

#include <QObject>
#include <QPoint>
#include <QVector>

// Piece types
enum class Piece : int {
    Empty = 0,
    Red = 1,
    Black = 2,
    RedKing = 3,
    BlackKing = 4
};

// Player colors
enum class PlayerColor : int {
    None = 0,
    Red = 1,
    Black = 2
};

// Move structure
struct Move {
    QPoint from;
    QPoint to;
    QVector<QPoint> captures; // Pieces captured during this move (for multi-jumps)
    
    bool isValid() const { return from != QPoint(-1, -1); }
    static Move invalid() { return {{-1, -1}, {-1, -1}, {}}; }
    
    bool operator==(const Move& other) const {
        return from == other.from && to == other.to;
    }
};

class CheckersGame : public QObject
{
    Q_OBJECT
    
public:
    static const int BOARD_SIZE = 8;
    
    explicit CheckersGame(QObject *parent = nullptr);
    
    // Game state
    void resetGame();
    Piece pieceAt(int row, int col) const;
    Piece pieceAt(const QPoint& pos) const;
    PlayerColor currentPlayer() const { return m_currentPlayer; }
    PlayerColor winner() const { return m_winner; }
    bool isGameOver() const { return m_winner != PlayerColor::None; }
    
    // Move validation and execution
    QVector<Move> getValidMoves(const QPoint& from) const;
    QVector<QPoint> getAllMovablePieces(PlayerColor player) const;
    bool isValidMove(const Move& move) const;
    bool makeMove(const Move& move);
    
    // Utility
    bool isPlayerPiece(const QPoint& pos, PlayerColor player) const;
    static PlayerColor pieceOwner(Piece piece);
    static bool isKing(Piece piece);
    
    // Serialization for network
    QByteArray serialize() const;
    void deserialize(const QByteArray& data);
    
signals:
    void boardChanged();
    void turnChanged(PlayerColor player);
    void gameOver(PlayerColor winner);
    void piecesCaptured(const QVector<QPoint>& positions);
    void pieceCrowned(const QPoint& position);
    
private:
    Piece m_board[BOARD_SIZE][BOARD_SIZE];
    PlayerColor m_currentPlayer;
    PlayerColor m_winner;
    
    void initializeBoard();
    void switchPlayer();
    void checkForWinner();
    bool canCapture(const QPoint& from, PlayerColor player) const;
    bool playerHasCapture(PlayerColor player) const;
    QVector<Move> getCaptureMoves(const QPoint& from) const;
    QVector<Move> getSimpleMoves(const QPoint& from) const;
    void findMultiJumps(const QPoint& current, const QPoint& original, 
                        Piece piece, QVector<QPoint>& captured,
                        QVector<Move>& moves, Piece tempBoard[BOARD_SIZE][BOARD_SIZE]) const;
    bool isValidPosition(const QPoint& pos) const;
    bool isEmpty(const QPoint& pos) const;
    bool isOpponent(const QPoint& pos, PlayerColor player) const;
};

#endif // CHECKERSGAME_H
