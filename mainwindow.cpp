#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "connectiondialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_game(new CheckersGame(this))
    , m_boardWidget(new CheckerBoardWidget(this))
    , m_networkManager(new NetworkManager(this))
{
    ui->setupUi(this);
    
    setWindowTitle(tr("LAN Checkers"));
    setMinimumSize(900, 700);
    
    setupUI();
    setupMenus();
    setupConnections();
    
    // Initial state
    m_boardWidget->setGame(m_game);
    m_boardWidget->setInteractive(false);
    updateStatus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Left side - Game board
    QVBoxLayout* leftLayout = new QVBoxLayout();
    
    // Turn indicator
    m_turnLabel = new QLabel(tr("Connect to start playing"));
    m_turnLabel->setAlignment(Qt::AlignCenter);
    m_turnLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px;");
    leftLayout->addWidget(m_turnLabel);
    
    // Checker board
    leftLayout->addWidget(m_boardWidget, 1);
    
    mainLayout->addLayout(leftLayout, 3);
    
    // Right side - Info and chat
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);
    
    // Connection info
    QGroupBox* connectionGroup = new QGroupBox(tr("Connection"));
    QVBoxLayout* connectionLayout = new QVBoxLayout(connectionGroup);
    
    m_statusLabel = new QLabel(tr("Not connected"));
    m_statusLabel->setStyleSheet("font-weight: bold;");
    connectionLayout->addWidget(m_statusLabel);
    
    m_playerInfoLabel = new QLabel();
    connectionLayout->addWidget(m_playerInfoLabel);
    
    m_opponentInfoLabel = new QLabel();
    connectionLayout->addWidget(m_opponentInfoLabel);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton(tr("Connect"));
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnect);
    buttonLayout->addWidget(m_connectButton);
    
    m_newGameButton = new QPushButton(tr("New Game"));
    m_newGameButton->setEnabled(false);
    connect(m_newGameButton, &QPushButton::clicked, this, &MainWindow::onNewGame);
    buttonLayout->addWidget(m_newGameButton);
    
    connectionLayout->addLayout(buttonLayout);
    rightLayout->addWidget(connectionGroup);
    
    // Game rules info
    QGroupBox* rulesGroup = new QGroupBox(tr("How to Play"));
    QVBoxLayout* rulesLayout = new QVBoxLayout(rulesGroup);
    QLabel* rulesLabel = new QLabel(
        tr("• Click and drag your pieces to move\n"
           "• Capture by jumping over opponent pieces\n"
           "• Multiple jumps are possible\n"
           "• Reach the opposite end to become a King\n"
           "• Kings can move backwards\n"
           "• Red moves first"));
    rulesLabel->setWordWrap(true);
    rulesLabel->setStyleSheet("color: gray;");
    rulesLayout->addWidget(rulesLabel);
    rightLayout->addWidget(rulesGroup);
    
    // Chat
    QGroupBox* chatGroup = new QGroupBox(tr("Chat"));
    QVBoxLayout* chatLayout = new QVBoxLayout(chatGroup);
    
    m_chatDisplay = new QTextEdit();
    m_chatDisplay->setReadOnly(true);
    m_chatDisplay->setMinimumHeight(150);
    chatLayout->addWidget(m_chatDisplay);
    
    QHBoxLayout* chatInputLayout = new QHBoxLayout();
    m_chatInput = new QLineEdit();
    m_chatInput->setPlaceholderText(tr("Type a message..."));
    m_chatInput->setEnabled(false);
    connect(m_chatInput, &QLineEdit::returnPressed, this, &MainWindow::onSendChat);
    chatInputLayout->addWidget(m_chatInput);
    
    m_sendChatButton = new QPushButton(tr("Send"));
    m_sendChatButton->setEnabled(false);
    connect(m_sendChatButton, &QPushButton::clicked, this, &MainWindow::onSendChat);
    chatInputLayout->addWidget(m_sendChatButton);
    
    chatLayout->addLayout(chatInputLayout);
    rightLayout->addWidget(chatGroup, 1);
    
    mainLayout->addLayout(rightLayout, 1);
    
    setCentralWidget(centralWidget);
}

void MainWindow::setupMenus()
{
    // Game menu
    QMenu* gameMenu = menuBar()->addMenu(tr("&Game"));
    
    QAction* connectAction = gameMenu->addAction(tr("&Connect..."));
    connect(connectAction, &QAction::triggered, this, &MainWindow::onConnect);
    
    QAction* disconnectAction = gameMenu->addAction(tr("&Disconnect"));
    connect(disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnect);
    
    gameMenu->addSeparator();
    
    QAction* newGameAction = gameMenu->addAction(tr("&New Game"));
    newGameAction->setShortcut(QKeySequence::New);
    connect(newGameAction, &QAction::triggered, this, &MainWindow::onNewGame);
    
    gameMenu->addSeparator();
    
    QAction* exitAction = gameMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    
    QAction* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About LAN Checkers"),
            tr("LAN Checkers v1.0\n\n"
               "A multiplayer checkers game that you can play\n"
               "with friends over your local network (WiFi/LAN).\n\n"
               "Built with Qt and C++."));
    });
}

void MainWindow::setupConnections()
{
    // Network manager signals
    connect(m_networkManager, &NetworkManager::connected, 
            this, &MainWindow::onConnected);
    connect(m_networkManager, &NetworkManager::disconnected, 
            this, &MainWindow::onDisconnected);
    connect(m_networkManager, &NetworkManager::connectionError, 
            this, &MainWindow::onConnectionError);
    connect(m_networkManager, &NetworkManager::opponentConnected, 
            this, &MainWindow::onOpponentConnected);
    connect(m_networkManager, &NetworkManager::opponentDisconnected, 
            this, &MainWindow::onOpponentDisconnected);
    connect(m_networkManager, &NetworkManager::moveReceived, 
            this, &MainWindow::onMoveReceived);
    connect(m_networkManager, &NetworkManager::gameStateReceived, 
            this, &MainWindow::onGameStateReceived);
    connect(m_networkManager, &NetworkManager::gameResetReceived, 
            this, &MainWindow::onGameResetReceived);
    connect(m_networkManager, &NetworkManager::chatMessageReceived, 
            this, &MainWindow::onChatMessageReceived);
    
    // Game signals
    connect(m_game, &CheckersGame::turnChanged, 
            this, &MainWindow::onTurnChanged);
    connect(m_game, &CheckersGame::gameOver, 
            this, &MainWindow::onGameOver);
    
    // Board widget signals
    connect(m_boardWidget, &CheckerBoardWidget::moveRequested, 
            this, &MainWindow::onMoveRequested);
}

void MainWindow::onConnect()
{
    ConnectionDialog dialog(m_networkManager, this);
    
    connect(&dialog, &ConnectionDialog::hostRequested, 
            this, [this](const QString& name, quint16 port) {
        m_playerName = name;
        if (!m_networkManager->hostGame(name, port)) {
            // Error is emitted by networkManager
        }
    });
    
    connect(&dialog, &ConnectionDialog::joinRequested, 
            this, [this](const QString& name, const QString& host, quint16 port) {
        m_playerName = name;
        if (!m_networkManager->joinGame(QHostAddress(host), port)) {
            // Error is emitted by networkManager
        }
    });
    
    dialog.exec();
    updateStatus();
}

void MainWindow::onDisconnect()
{
    m_networkManager->disconnect();
    m_gameStarted = false;
    m_boardWidget->setInteractive(false);
    m_boardWidget->clearHighlights();
    m_game->resetGame();
    updateStatus();
    appendChatMessage("", tr("Disconnected from game."), true);
}

void MainWindow::onConnected()
{
    updateStatus();
    
    if (m_networkManager->isHost()) {
        appendChatMessage("", tr("Hosting game. Waiting for opponent..."), true);
        m_statusLabel->setText(tr("Hosting - Waiting for opponent"));
    } else {
        appendChatMessage("", tr("Connected to host."), true);
    }
}

void MainWindow::onDisconnected()
{
    m_gameStarted = false;
    m_boardWidget->setInteractive(false);
    updateStatus();
    appendChatMessage("", tr("Connection lost."), true);
}

void MainWindow::onConnectionError(const QString& error)
{
    QMessageBox::warning(this, tr("Connection Error"), error);
    updateStatus();
}

void MainWindow::onOpponentConnected(const QString& name)
{
    updateStatus();
    appendChatMessage("", tr("%1 has joined the game.").arg(name), true);
    
    // Start the game
    startGame();
}

void MainWindow::onOpponentDisconnected()
{
    m_gameStarted = false;
    m_boardWidget->setInteractive(false);
    updateStatus();
    appendChatMessage("", tr("Opponent disconnected."), true);
    
    QMessageBox::information(this, tr("Opponent Left"), 
        tr("Your opponent has disconnected from the game."));
}

void MainWindow::startGame()
{
    m_gameStarted = true;
    m_game->resetGame();
    
    // Set up board for local player
    m_boardWidget->setLocalPlayerColor(m_networkManager->localPlayerColor());
    
    // Host sends initial game state
    if (m_networkManager->isHost()) {
        m_networkManager->sendGameState(m_game);
        m_networkManager->sendGameStart();
    }
    
    updateGameControls();
    updateStatus();
    
    appendChatMessage("", tr("Game started! %1 goes first.")
        .arg(m_game->currentPlayer() == PlayerColor::Red ? tr("Red") : tr("Black")), true);
}

void MainWindow::onMoveReceived(const Move& move)
{
    // Apply opponent's move
    if (m_game->makeMove(move)) {
        updateGameControls();
    }
}

void MainWindow::onGameStateReceived(const QByteArray& state)
{
    m_game->deserialize(state);
    updateGameControls();
}

void MainWindow::onGameResetReceived()
{
    m_game->resetGame();
    
    // Host sends new game state
    if (m_networkManager->isHost()) {
        m_networkManager->sendGameState(m_game);
    }
    
    updateGameControls();
    appendChatMessage("", tr("Game has been reset."), true);
}

void MainWindow::onNewGame()
{
    if (!m_networkManager->isConnected()) {
        m_game->resetGame();
        return;
    }
    
    if (QMessageBox::question(this, tr("New Game"),
            tr("Start a new game? This will reset the current game."),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        
        m_game->resetGame();
        m_networkManager->sendGameReset();
        m_networkManager->sendGameState(m_game);
        
        updateGameControls();
        appendChatMessage("", tr("Game has been reset."), true);
    }
}

void MainWindow::onTurnChanged(PlayerColor player)
{
    updateGameControls();
    
    QString playerName = (player == PlayerColor::Red) ? tr("Red") : tr("Black");
    
    if (m_networkManager->isConnected()) {
        bool isMyTurn = (player == m_networkManager->localPlayerColor());
        if (isMyTurn) {
            m_turnLabel->setText(tr("Your turn (%1)").arg(playerName));
            m_turnLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px; color: green;");
        } else {
            m_turnLabel->setText(tr("Opponent's turn (%1)").arg(playerName));
            m_turnLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px; color: gray;");
        }
    } else {
        m_turnLabel->setText(tr("%1's turn").arg(playerName));
        m_turnLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px;");
    }
}

void MainWindow::onGameOver(PlayerColor winner)
{
    m_boardWidget->setInteractive(false);
    m_boardWidget->clearHighlights();
    
    QString winnerName = (winner == PlayerColor::Red) ? tr("Red") : tr("Black");
    
    bool isLocalWinner = (winner == m_networkManager->localPlayerColor());
    
    QString message;
    if (m_networkManager->isConnected()) {
        if (isLocalWinner) {
            message = tr("Congratulations! You won!");
            m_turnLabel->setText(tr("You Won!"));
            m_turnLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px; color: gold;");
        } else {
            message = tr("%1 wins! Better luck next time.").arg(m_networkManager->opponentName());
            m_turnLabel->setText(tr("You Lost"));
            m_turnLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px; color: red;");
        }
    } else {
        message = tr("%1 wins!").arg(winnerName);
        m_turnLabel->setText(tr("%1 Wins!").arg(winnerName));
    }
    
    appendChatMessage("", message, true);
    
    QMessageBox::information(this, tr("Game Over"), message);
}

void MainWindow::onMoveRequested(const Move& move)
{
    // Check if it's our turn
    if (!m_gameStarted || m_game->currentPlayer() != m_networkManager->localPlayerColor()) {
        return;
    }
    
    // Make the move locally
    if (m_game->makeMove(move)) {
        // Send move to opponent
        m_networkManager->sendMove(move);
        updateGameControls();
    }
}

void MainWindow::updateGameControls()
{
    if (!m_gameStarted || m_game->isGameOver()) {
        m_boardWidget->setInteractive(false);
        m_boardWidget->clearHighlights();
        return;
    }
    
    bool isMyTurn = (m_game->currentPlayer() == m_networkManager->localPlayerColor());
    m_boardWidget->setInteractive(isMyTurn);
    
    if (isMyTurn) {
        // Highlight pieces that can move
        QVector<QPoint> movable = m_game->getAllMovablePieces(m_networkManager->localPlayerColor());
        m_boardWidget->highlightMovablePieces(movable);
    } else {
        m_boardWidget->clearHighlights();
    }
}

void MainWindow::updateStatus()
{
    bool connected = m_networkManager->isConnected();
    
    m_chatInput->setEnabled(connected);
    m_sendChatButton->setEnabled(connected);
    m_newGameButton->setEnabled(connected && m_gameStarted);
    
    if (!connected && !m_networkManager->isHost()) {
        m_statusLabel->setText(tr("Not connected"));
        m_statusLabel->setStyleSheet("font-weight: bold; color: gray;");
        m_playerInfoLabel->clear();
        m_opponentInfoLabel->clear();
        m_connectButton->setText(tr("Connect"));
        m_turnLabel->setText(tr("Connect to start playing"));
        m_turnLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px;");
    } else if (m_networkManager->isHost() && !connected) {
        m_statusLabel->setText(tr("Hosting - Waiting for opponent"));
        m_statusLabel->setStyleSheet("font-weight: bold; color: orange;");
        m_playerInfoLabel->setText(tr("You: %1 (Red)").arg(m_playerName));
        m_opponentInfoLabel->setText(tr("Opponent: Waiting..."));
        m_connectButton->setText(tr("Disconnect"));
    } else if (connected) {
        m_statusLabel->setText(tr("Connected"));
        m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
        
        QString myColor = (m_networkManager->localPlayerColor() == PlayerColor::Red) ? tr("Red") : tr("Black");
        QString oppColor = (m_networkManager->localPlayerColor() == PlayerColor::Red) ? tr("Black") : tr("Red");
        
        m_playerInfoLabel->setText(tr("You: %1 (%2)").arg(m_playerName, myColor));
        m_opponentInfoLabel->setText(tr("Opponent: %1 (%2)")
            .arg(m_networkManager->opponentName(), oppColor));
        m_connectButton->setText(tr("Disconnect"));
    }
    
    // Update connect button action
    disconnect(m_connectButton, &QPushButton::clicked, nullptr, nullptr);
    if (connected || m_networkManager->isHost()) {
        connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    } else {
        connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnect);
    }
}

void MainWindow::onSendChat()
{
    QString message = m_chatInput->text().trimmed();
    if (message.isEmpty()) return;
    
    m_chatInput->clear();
    
    // Send to opponent
    m_networkManager->sendChatMessage(message);
    
    // Display locally
    appendChatMessage(m_playerName, message);
}

void MainWindow::onChatMessageReceived(const QString& from, const QString& message)
{
    appendChatMessage(from, message);
}

void MainWindow::appendChatMessage(const QString& from, const QString& message, bool isSystem)
{
    QString formatted;
    
    if (isSystem) {
        formatted = QString("<i style='color: gray;'>%1</i>").arg(message);
    } else if (from == m_playerName) {
        formatted = QString("<b style='color: blue;'>%1:</b> %2").arg(from, message);
    } else {
        formatted = QString("<b style='color: green;'>%1:</b> %2").arg(from, message);
    }
    
    m_chatDisplay->append(formatted);
}
