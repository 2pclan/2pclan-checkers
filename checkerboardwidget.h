#ifndef CHECKERBOARDWIDGET_H
#define CHECKERBOARDWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QVector>
#include "checkersgame.h"

class CheckerBoardWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit CheckerBoardWidget(QWidget *parent = nullptr);
    
    void setGame(CheckersGame* game);
    void setLocalPlayerColor(PlayerColor color);
    void setInteractive(bool interactive);
    void setFlipped(bool flipped);
    
    // Highlight valid moves
    void clearHighlights();
    void highlightValidMoves(const QVector<Move>& moves);
    void highlightMovablePieces(const QVector<QPoint>& pieces);
    
signals:
    void squareClicked(const QPoint& pos);
    void moveRequested(const Move& move);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private:
    // Drawing helpers
    void drawBoard(QPainter& painter);
    void drawPieces(QPainter& painter);
    void drawHighlights(QPainter& painter);
    void drawDraggedPiece(QPainter& painter);
    void drawPiece(QPainter& painter, const QRect& rect, Piece piece, bool isGhost = false);
    
    // Coordinate conversion
    QPoint boardToScreen(const QPoint& boardPos) const;
    QPoint screenToBoard(const QPoint& screenPos) const;
    QRect squareRect(const QPoint& boardPos) const;
    int squareSize() const;
    QPoint adjustForFlip(const QPoint& pos) const;
    
    // Game reference
    CheckersGame* m_game = nullptr;
    PlayerColor m_localColor = PlayerColor::None;
    bool m_interactive = true;
    bool m_flipped = false;
    
    // Selection state
    QPoint m_selectedSquare{-1, -1};
    QVector<Move> m_validMoves;
    QVector<QPoint> m_movablePieces;
    
    // Drag state
    bool m_dragging = false;
    QPoint m_dragStart;
    QPoint m_dragCurrent;
    Piece m_draggedPiece = Piece::Empty;
    
    // Visual settings
    QColor m_lightSquareColor{240, 217, 181};
    QColor m_darkSquareColor{181, 136, 99};
    QColor m_highlightColor{255, 255, 0, 100};
    QColor m_selectedColor{0, 255, 0, 150};
    QColor m_validMoveColor{0, 200, 0, 100};
    QColor m_redPieceColor{200, 50, 50};
    QColor m_blackPieceColor{40, 40, 40};
    QColor m_kingMarkerColor{255, 215, 0};
    
    int m_boardMargin = 10;
};

#endif // CHECKERBOARDWIDGET_H
