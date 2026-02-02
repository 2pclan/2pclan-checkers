#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include "checkersgame.h"
#include "checkerboardwidget.h"
#include "networkmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Menu actions
    void onNewGame();
    void onConnect();
    void onDisconnect();
    
    // Network events
    void onConnected();
    void onDisconnected();
    void onConnectionError(const QString& error);
    void onOpponentConnected(const QString& name);
    void onOpponentDisconnected();
    void onMoveReceived(const Move& move);
    void onGameStateReceived(const QByteArray& state);
    void onGameResetReceived();
    
    // Game events
    void onTurnChanged(PlayerColor player);
    void onGameOver(PlayerColor winner);
    void onMoveRequested(const Move& move);
    
    // Chat
    void onSendChat();
    void onChatMessageReceived(const QString& from, const QString& message);

private:
    void setupUI();
    void setupMenus();
    void setupConnections();
    void updateStatus();
    void updateGameControls();
    void appendChatMessage(const QString& from, const QString& message, bool isSystem = false);
    void startGame();
    
    Ui::MainWindow *ui;
    
    // Game components
    CheckersGame* m_game;
    CheckerBoardWidget* m_boardWidget;
    NetworkManager* m_networkManager;
    
    // UI components
    QLabel* m_statusLabel;
    QLabel* m_turnLabel;
    QLabel* m_playerInfoLabel;
    QLabel* m_opponentInfoLabel;
    QTextEdit* m_chatDisplay;
    QLineEdit* m_chatInput;
    QPushButton* m_sendChatButton;
    QPushButton* m_newGameButton;
    QPushButton* m_connectButton;
    
    // State
    bool m_gameStarted = false;
    QString m_playerName;
};

#endif // MAINWINDOW_H
