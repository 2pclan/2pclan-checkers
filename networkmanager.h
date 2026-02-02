#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include "checkersgame.h"

// Message types for network protocol
enum class MessageType : quint8 {
    GameState = 1,      // Full game state sync
    Move = 2,           // A move was made
    ChatMessage = 3,    // Chat message
    PlayerReady = 4,    // Player is ready to start
    GameStart = 5,      // Game is starting
    GameReset = 6,      // Reset the game
    Ping = 7,           // Keep-alive ping
    Pong = 8,           // Keep-alive response
    Disconnect = 9      // Player disconnecting
};

// Network role
enum class NetworkRole {
    None,
    Host,
    Client
};

// Discovered peer info
struct PeerInfo {
    QString name;
    QHostAddress address;
    quint16 port;
    qint64 lastSeen;
};

class NetworkManager : public QObject
{
    Q_OBJECT
    
public:
    static constexpr quint16 DEFAULT_PORT = 45678;
    static constexpr quint16 DISCOVERY_PORT = 45679;
    static constexpr int DISCOVERY_INTERVAL_MS = 2000;
    static constexpr int PEER_TIMEOUT_MS = 6000;
    
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();
    
    // Connection management
    bool hostGame(const QString& playerName, quint16 port = DEFAULT_PORT);
    bool joinGame(const QHostAddress& hostAddress, quint16 port = DEFAULT_PORT);
    void disconnect();
    
    // State
    bool isConnected() const { return m_connected; }
    bool isHost() const { return m_role == NetworkRole::Host; }
    NetworkRole role() const { return m_role; }
    QString playerName() const { return m_playerName; }
    QString opponentName() const { return m_opponentName; }
    PlayerColor localPlayerColor() const { return m_localColor; }
    
    // Discovery
    void startDiscovery();
    void stopDiscovery();
    QList<PeerInfo> discoveredPeers() const { return m_discoveredPeers.values(); }
    
    // Game communication
    void sendMove(const Move& move);
    void sendGameState(const CheckersGame* game);
    void sendChatMessage(const QString& message);
    void sendGameReset();
    void sendPlayerReady();
    void sendGameStart();
    
    // Utility
    static QString getLocalIPAddress();
    
signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    
    void moveReceived(const Move& move);
    void gameStateReceived(const QByteArray& state);
    void chatMessageReceived(const QString& from, const QString& message);
    void playerReadyReceived();
    void gameStartReceived();
    void gameResetReceived();
    
    void peerDiscovered(const PeerInfo& peer);
    void peerLost(const QString& peerId);
    void peersChanged();
    
    void opponentConnected(const QString& name);
    void opponentDisconnected();
    
private slots:
    void onNewConnection();
    void onClientConnected();
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    
    void onDiscoveryReadyRead();
    void announcePresence();
    void cleanupStalePeers();
    void sendPing();
    
private:
    void processMessage(const QByteArray& data);
    void sendMessage(MessageType type, const QByteArray& payload = QByteArray());
    QByteArray createPacket(MessageType type, const QByteArray& payload);
    
    // TCP
    QTcpServer* m_server = nullptr;
    QTcpSocket* m_socket = nullptr;
    QByteArray m_readBuffer;
    
    // UDP Discovery
    QUdpSocket* m_discoverySocket = nullptr;
    QTimer* m_discoveryTimer = nullptr;
    QTimer* m_cleanupTimer = nullptr;
    QMap<QString, PeerInfo> m_discoveredPeers;
    
    // Keep-alive
    QTimer* m_pingTimer = nullptr;
    
    // State
    NetworkRole m_role = NetworkRole::None;
    bool m_connected = false;
    QString m_playerName;
    QString m_opponentName;
    PlayerColor m_localColor = PlayerColor::None;
    quint16 m_hostPort = DEFAULT_PORT;
};

#endif // NETWORKMANAGER_H
