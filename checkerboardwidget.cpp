#include "checkerboardwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

CheckerBoardWidget::CheckerBoardWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(400, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void CheckerBoardWidget::setGame(CheckersGame* game)
{
    if (m_game) {
        disconnect(m_game, nullptr, this, nullptr);
    }
    
    m_game = game;
    
    if (m_game) {
        connect(m_game, &CheckersGame::boardChanged, this, [this]() {
            clearHighlights();
            update();
        });
    }
    
    update();
}

void CheckerBoardWidget::setLocalPlayerColor(PlayerColor color)
{
    m_localColor = color;
    // Flip board if playing as black (so your pieces are at the bottom)
    setFlipped(color == PlayerColor::Black);
}

void CheckerBoardWidget::setInteractive(bool interactive)
{
    m_interactive = interactive;
    if (!interactive) {
        clearHighlights();
    }
    update();
}

void CheckerBoardWidget::setFlipped(bool flipped)
{
    m_flipped = flipped;
    update();
}

void CheckerBoardWidget::clearHighlights()
{
    m_selectedSquare = QPoint(-1, -1);
    m_validMoves.clear();
    m_movablePieces.clear();
    m_dragging = false;
    m_draggedPiece = Piece::Empty;
    update();
}

void CheckerBoardWidget::highlightValidMoves(const QVector<Move>& moves)
{
    m_validMoves = moves;
    update();
}

void CheckerBoardWidget::highlightMovablePieces(const QVector<QPoint>& pieces)
{
    m_movablePieces = pieces;
    update();
}

int CheckerBoardWidget::squareSize() const
{
    int availableSize = qMin(width(), height()) - 2 * m_boardMargin;
    return availableSize / CheckersGame::BOARD_SIZE;
}

QPoint CheckerBoardWidget::adjustForFlip(const QPoint& pos) const
{
    if (m_flipped) {
        return QPoint(CheckersGame::BOARD_SIZE - 1 - pos.x(), 
                      CheckersGame::BOARD_SIZE - 1 - pos.y());
    }
    return pos;
}

QPoint CheckerBoardWidget::boardToScreen(const QPoint& boardPos) const
{
    QPoint adjusted = adjustForFlip(boardPos);
    int size = squareSize();
    int boardPixelSize = size * CheckersGame::BOARD_SIZE;
    int offsetX = (width() - boardPixelSize) / 2;
    int offsetY = (height() - boardPixelSize) / 2;
    
    return QPoint(offsetX + adjusted.x() * size + size / 2,
                  offsetY + adjusted.y() * size + size / 2);
}

QPoint CheckerBoardWidget::screenToBoard(const QPoint& screenPos) const
{
    int size = squareSize();
    int boardPixelSize = size * CheckersGame::BOARD_SIZE;
    int offsetX = (width() - boardPixelSize) / 2;
    int offsetY = (height() - boardPixelSize) / 2;
    
    int col = (screenPos.x() - offsetX) / size;
    int row = (screenPos.y() - offsetY) / size;
    
    if (col < 0 || col >= CheckersGame::BOARD_SIZE || 
        row < 0 || row >= CheckersGame::BOARD_SIZE) {
        return QPoint(-1, -1);
    }
    
    return adjustForFlip(QPoint(col, row));
}

QRect CheckerBoardWidget::squareRect(const QPoint& boardPos) const
{
    QPoint adjusted = adjustForFlip(boardPos);
    int size = squareSize();
    int boardPixelSize = size * CheckersGame::BOARD_SIZE;
    int offsetX = (width() - boardPixelSize) / 2;
    int offsetY = (height() - boardPixelSize) / 2;
    
    return QRect(offsetX + adjusted.x() * size,
                 offsetY + adjusted.y() * size,
                 size, size);
}

void CheckerBoardWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawBoard(painter);
    drawHighlights(painter);
    drawPieces(painter);
    drawDraggedPiece(painter);
}

void CheckerBoardWidget::drawBoard(QPainter& painter)
{
    int size = squareSize();
    int boardPixelSize = size * CheckersGame::BOARD_SIZE;
    int offsetX = (width() - boardPixelSize) / 2;
    int offsetY = (height() - boardPixelSize) / 2;
    
    // Draw board border
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(offsetX - 2, offsetY - 2, 
                     boardPixelSize + 4, boardPixelSize + 4);
    
    // Draw squares
    for (int row = 0; row < CheckersGame::BOARD_SIZE; ++row) {
        for (int col = 0; col < CheckersGame::BOARD_SIZE; ++col) {
            QRect rect(offsetX + col * size, offsetY + row * size, size, size);
            
            bool isDark = (row + col) % 2 == 1;
            painter.fillRect(rect, isDark ? m_darkSquareColor : m_lightSquareColor);
        }
    }
}

void CheckerBoardWidget::drawHighlights(QPainter& painter)
{
    // Highlight movable pieces
    for (const QPoint& pos : m_movablePieces) {
        QRect rect = squareRect(pos);
        painter.fillRect(rect, m_highlightColor);
    }
    
    // Highlight selected square
    if (m_selectedSquare.x() >= 0) {
        QRect rect = squareRect(m_selectedSquare);
        painter.fillRect(rect, m_selectedColor);
    }
    
    // Highlight valid move destinations
    for (const Move& move : m_validMoves) {
        QRect rect = squareRect(move.to);
        painter.fillRect(rect, m_validMoveColor);
        
        // Draw a circle indicator
        painter.setPen(QPen(QColor(0, 150, 0), 2));
        painter.setBrush(QColor(0, 200, 0, 80));
        int margin = squareSize() / 4;
        painter.drawEllipse(rect.adjusted(margin, margin, -margin, -margin));
    }
}

void CheckerBoardWidget::drawPieces(QPainter& painter)
{
    if (!m_game) return;
    
    for (int row = 0; row < CheckersGame::BOARD_SIZE; ++row) {
        for (int col = 0; col < CheckersGame::BOARD_SIZE; ++col) {
            QPoint pos(col, row);
            Piece piece = m_game->pieceAt(pos);
            
            if (piece == Piece::Empty) continue;
            
            // Don't draw the piece being dragged at its original position
            if (m_dragging && pos == m_selectedSquare) {
                // Draw ghost piece
                drawPiece(painter, squareRect(pos), piece, true);
                continue;
            }
            
            drawPiece(painter, squareRect(pos), piece);
        }
    }
}

void CheckerBoardWidget::drawPiece(QPainter& painter, const QRect& rect, Piece piece, bool isGhost)
{
    if (piece == Piece::Empty) return;
    
    int margin = rect.width() / 8;
    QRect pieceRect = rect.adjusted(margin, margin, -margin, -margin);
    
    QColor color;
    if (piece == Piece::Red || piece == Piece::RedKing) {
        color = m_redPieceColor;
    } else {
        color = m_blackPieceColor;
    }
    
    if (isGhost) {
        color.setAlpha(80);
    }
    
    // Draw piece shadow
    if (!isGhost) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 50));
        painter.drawEllipse(pieceRect.translated(3, 3));
    }
    
    // Draw piece body
    painter.setPen(QPen(color.darker(130), 2));
    painter.setBrush(color);
    painter.drawEllipse(pieceRect);
    
    // Draw inner ring for 3D effect
    int innerMargin = pieceRect.width() / 6;
    QRect innerRect = pieceRect.adjusted(innerMargin, innerMargin, -innerMargin, -innerMargin);
    painter.setPen(QPen(color.lighter(120), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(innerRect);
    
    // Draw king crown
    if (CheckersGame::isKing(piece) && !isGhost) {
        painter.setPen(QPen(m_kingMarkerColor.darker(110), 2));
        painter.setBrush(m_kingMarkerColor);
        
        int crownSize = pieceRect.width() / 3;
        QRect crownRect(pieceRect.center().x() - crownSize / 2,
                        pieceRect.center().y() - crownSize / 2,
                        crownSize, crownSize);
        
        // Draw a star/crown symbol
        QPolygon crown;
        int cx = crownRect.center().x();
        int cy = crownRect.center().y();
        int r = crownSize / 2;
        
        for (int i = 0; i < 5; ++i) {
            double angle = -M_PI / 2 + i * 2 * M_PI / 5;
            crown << QPoint(cx + r * qCos(angle), cy + r * qSin(angle));
            
            angle += M_PI / 5;
            crown << QPoint(cx + r * 0.4 * qCos(angle), cy + r * 0.4 * qSin(angle));
        }
        
        painter.drawPolygon(crown);
    }
}

void CheckerBoardWidget::drawDraggedPiece(QPainter& painter)
{
    if (!m_dragging || m_draggedPiece == Piece::Empty) return;
    
    int size = squareSize();
    int margin = size / 8;
    QRect pieceRect(m_dragCurrent.x() - size / 2 + margin,
                    m_dragCurrent.y() - size / 2 + margin,
                    size - 2 * margin, size - 2 * margin);
    
    drawPiece(painter, QRect(m_dragCurrent.x() - size / 2,
                              m_dragCurrent.y() - size / 2,
                              size, size), m_draggedPiece);
}

void CheckerBoardWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_interactive || !m_game || event->button() != Qt::LeftButton) {
        return;
    }
    
    QPoint boardPos = screenToBoard(event->pos());
    
    if (boardPos.x() < 0) {
        clearHighlights();
        return;
    }
    
    // Check if clicking on own piece
    if (m_game->isPlayerPiece(boardPos, m_localColor)) {
        // Check if this piece can move
        QVector<Move> moves = m_game->getValidMoves(boardPos);
        
        // Only allow selecting pieces that can actually move
        QVector<QPoint> movable = m_game->getAllMovablePieces(m_localColor);
        if (!movable.contains(boardPos)) {
            return;
        }
        
        m_selectedSquare = boardPos;
        m_validMoves = moves;
        m_dragging = true;
        m_dragStart = event->pos();
        m_dragCurrent = event->pos();
        m_draggedPiece = m_game->pieceAt(boardPos);
        
        update();
    }
    // Check if clicking on valid move destination
    else if (m_selectedSquare.x() >= 0) {
        for (const Move& move : m_validMoves) {
            if (move.to == boardPos) {
                emit moveRequested(move);
                clearHighlights();
                return;
            }
        }
        clearHighlights();
    }
    
    emit squareClicked(boardPos);
}

void CheckerBoardWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_dragging) return;
    
    QPoint boardPos = screenToBoard(event->pos());
    
    // Check if releasing on valid destination
    if (boardPos.x() >= 0) {
        for (const Move& move : m_validMoves) {
            if (move.to == boardPos) {
                emit moveRequested(move);
                clearHighlights();
                return;
            }
        }
    }
    
    // Invalid drop - keep piece selected but stop dragging
    m_dragging = false;
    m_draggedPiece = Piece::Empty;
    update();
}

void CheckerBoardWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_dragCurrent = event->pos();
        update();
    }
}

void CheckerBoardWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    update();
}
