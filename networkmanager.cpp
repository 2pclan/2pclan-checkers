#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_discoverySocket(new QUdpSocket(this))
    , m_discoveryTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_pingTimer(new QTimer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    connect(m_discoverySocket, &QUdpSocket::readyRead, this, &NetworkManager::onDiscoveryReadyRead);
    connect(m_discoveryTimer, &QTimer::timeout, this, &NetworkManager::announcePresence);
    connect(m_cleanupTimer, &QTimer::timeout, this, &NetworkManager::cleanupStalePeers);
    connect(m_pingTimer, &QTimer::timeout, this, &NetworkManager::sendPing);
}

NetworkManager::~NetworkManager()
{
    disconnect();
}

QString NetworkManager::getLocalIPAddress()
{
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            iface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            
            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                QHostAddress addr = entry.ip();
                if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
                    !addr.isLoopback()) {
                    return addr.toString();
                }
            }
        }
    }
    return "127.0.0.1";
}

bool NetworkManager::hostGame(const QString& playerName, quint16 port)
{
    if (m_connected) {
        disconnect();
    }
    
    m_playerName = playerName;
    m_hostPort = port;
    m_role = NetworkRole::Host;
    m_localColor = PlayerColor::Red; // Host plays as Red
    
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit connectionError(tr("Failed to start server: %1").arg(m_server->errorString()));
        m_role = NetworkRole::None;
        return false;
    }
    
    // Start announcing presence for discovery
    startDiscovery();
    
    return true;
}

bool NetworkManager::joinGame(const QHostAddress& hostAddress, quint16 port)
{
    if (m_connected) {
        disconnect();
    }
    
    m_role = NetworkRole::Client;
    m_localColor = PlayerColor::Black; // Client plays as Black
    
    m_socket = new QTcpSocket(this);
    
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onClientConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &NetworkManager::onSocketError);
#endif
    
    m_socket->connectToHost(hostAddress, port);
    
    return true;
}

void NetworkManager::disconnect()
{
    // Stop discovery
    stopDiscovery();
    m_pingTimer->stop();
    
    // Send disconnect message if connected
    if (m_connected && m_socket) {
        sendMessage(MessageType::Disconnect);
        m_socket->flush();
        m_socket->waitForBytesWritten(1000);
    }
    
    // Close socket
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    
    // Stop server
    if (m_server->isListening()) {
        m_server->close();
    }
    
    m_connected = false;
    m_role = NetworkRole::None;
    m_opponentName.clear();
    m_readBuffer.clear();
}

void NetworkManager::startDiscovery()
{
    m_discoverySocket->bind(DISCOVERY_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_discoveryTimer->start(DISCOVERY_INTERVAL_MS);
    m_cleanupTimer->start(1000);
    
    // Announce immediately
    announcePresence();
}

void NetworkManager::stopDiscovery()
{
    m_discoveryTimer->stop();
    m_cleanupTimer->stop();
    m_discoverySocket->close();
    m_discoveredPeers.clear();
}

void NetworkManager::announcePresence()
{
    if (m_role != NetworkRole::Host || !m_server->isListening()) {
        return;
    }
    
    QJsonObject obj;
    obj["type"] = "CHECKERS_GAME";
    obj["name"] = m_playerName;
    obj["port"] = static_cast<int>(m_hostPort);
    
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    m_discoverySocket->writeDatagram(data, QHostAddress::Broadcast, DISCOVERY_PORT);
}

void NetworkManager::onDiscoveryReadyRead()
{
    while (m_discoverySocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_discoverySocket->pendingDatagramSize());
        QHostAddress sender;
        
        m_discoverySocket->readDatagram(datagram.data(), datagram.size(), &sender, nullptr);
        
        // Ignore our own broadcasts
        QString localIP = getLocalIPAddress();
        if (sender.toString() == localIP) {
            continue;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (!doc.isObject()) continue;
        
        QJsonObject obj = doc.object();
        if (obj["type"].toString() != "CHECKERS_GAME") continue;
        
        QString name = obj["name"].toString();
        quint16 port = static_cast<quint16>(obj["port"].toInt());
        
        if (name.isEmpty() || port == 0) continue;
        
        QString peerId = QString("%1:%2").arg(sender.toString()).arg(port);
        bool isNew = !m_discoveredPeers.contains(peerId);
        
        PeerInfo info;
        info.name = name;
        info.address = sender;
        info.port = port;
        info.lastSeen = QDateTime::currentMSecsSinceEpoch();
        
        m_discoveredPeers[peerId] = info;
        
        if (isNew) {
            emit peerDiscovered(info);
            emit peersChanged();
        }
    }
}

void NetworkManager::cleanupStalePeers()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList stale;
    
    for (auto it = m_discoveredPeers.begin(); it != m_discoveredPeers.end(); ++it) {
        if (now - it->lastSeen > PEER_TIMEOUT_MS) {
            stale.append(it.key());
        }
    }
    
    for (const QString& peerId : stale) {
        m_discoveredPeers.remove(peerId);
        emit peerLost(peerId);
    }
    
    if (!stale.isEmpty()) {
        emit peersChanged();
    }
}

void NetworkManager::onNewConnection()
{
    if (m_connected) {
        // Already have a player, reject
        QTcpSocket* pending = m_server->nextPendingConnection();
        pending->disconnectFromHost();
        pending->deleteLater();
        return;
    }
    
    m_socket = m_server->nextPendingConnection();
    
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &NetworkManager::onSocketError);
#endif
    
    m_connected = true;
    m_opponentName = m_socket->peerAddress().toString();
    
    // Stop accepting new connections by closing and reopening won't work
    // Instead we just reject additional connections in onNewConnection
    
    // Start keep-alive
    m_pingTimer->start(5000);
    
    // Send our player info
    QJsonObject info;
    info["name"] = m_playerName;
    sendMessage(MessageType::PlayerReady, QJsonDocument(info).toJson(QJsonDocument::Compact));
    
    emit connected();
    emit opponentConnected(m_opponentName);
}

void NetworkManager::onClientConnected()
{
    m_connected = true;
    
    // Start keep-alive
    m_pingTimer->start(5000);
    
    // Send our player info
    QJsonObject info;
    info["name"] = m_playerName;
    sendMessage(MessageType::PlayerReady, QJsonDocument(info).toJson(QJsonDocument::Compact));
    
    emit connected();
}

void NetworkManager::onReadyRead()
{
    if (!m_socket) return;
    
    m_readBuffer.append(m_socket->readAll());
    
    // Process complete messages (length-prefixed protocol)
    while (m_readBuffer.size() >= static_cast<int>(sizeof(quint32))) {
        QDataStream stream(m_readBuffer);
        stream.setVersion(QDataStream::Qt_5_15);
        
        quint32 packetSize;
        stream >> packetSize;
        
        if (m_readBuffer.size() < static_cast<int>(sizeof(quint32) + packetSize)) {
            break; // Wait for more data
        }
        
        QByteArray packet = m_readBuffer.mid(sizeof(quint32), packetSize);
        m_readBuffer.remove(0, sizeof(quint32) + packetSize);
        
        processMessage(packet);
    }
}

void NetworkManager::processMessage(const QByteArray& data)
{
    if (data.isEmpty()) return;
    
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_5_15);
    
    quint8 typeValue;
    stream >> typeValue;
    MessageType type = static_cast<MessageType>(typeValue);
    
    QByteArray payload;
    stream >> payload;
    
    switch (type) {
        case MessageType::GameState:
            emit gameStateReceived(payload);
            break;
            
        case MessageType::Move: {
            QDataStream moveStream(payload);
            moveStream.setVersion(QDataStream::Qt_5_15);
            
            int fromX, fromY, toX, toY;
            moveStream >> fromX >> fromY >> toX >> toY;
            
            Move move;
            move.from = QPoint(fromX, fromY);
            move.to = QPoint(toX, toY);
            
            emit moveReceived(move);
            break;
        }
            
        case MessageType::ChatMessage: {
            QString message = QString::fromUtf8(payload);
            emit chatMessageReceived(m_opponentName, message);
            break;
        }
            
        case MessageType::PlayerReady: {
            QJsonDocument doc = QJsonDocument::fromJson(payload);
            if (doc.isObject()) {
                m_opponentName = doc.object()["name"].toString();
            }
            emit playerReadyReceived();
            emit opponentConnected(m_opponentName);
            break;
        }
            
        case MessageType::GameStart:
            emit gameStartReceived();
            break;
            
        case MessageType::GameReset:
            emit gameResetReceived();
            break;
            
        case MessageType::Ping:
            sendMessage(MessageType::Pong);
            break;
            
        case MessageType::Pong:
            // Connection is alive
            break;
            
        case MessageType::Disconnect:
            onSocketDisconnected();
            break;
    }
}

void NetworkManager::sendMessage(MessageType type, const QByteArray& payload)
{
    if (!m_socket || !m_socket->isOpen()) return;
    
    QByteArray packet = createPacket(type, payload);
    m_socket->write(packet);
}

QByteArray NetworkManager::createPacket(MessageType type, const QByteArray& payload)
{
    QByteArray innerData;
    QDataStream innerStream(&innerData, QIODevice::WriteOnly);
    innerStream.setVersion(QDataStream::Qt_5_15);
    innerStream << static_cast<quint8>(type) << payload;
    
    QByteArray packet;
    QDataStream packetStream(&packet, QIODevice::WriteOnly);
    packetStream.setVersion(QDataStream::Qt_5_15);
    packetStream << static_cast<quint32>(innerData.size());
    packet.append(innerData);
    
    return packet;
}

void NetworkManager::sendMove(const Move& move)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << move.from.x() << move.from.y() << move.to.x() << move.to.y();
    
    sendMessage(MessageType::Move, payload);
}

void NetworkManager::sendGameState(const CheckersGame* game)
{
    if (!game) return;
    sendMessage(MessageType::GameState, game->serialize());
}

void NetworkManager::sendChatMessage(const QString& message)
{
    sendMessage(MessageType::ChatMessage, message.toUtf8());
}

void NetworkManager::sendGameReset()
{
    sendMessage(MessageType::GameReset);
}

void NetworkManager::sendPlayerReady()
{
    QJsonObject info;
    info["name"] = m_playerName;
    sendMessage(MessageType::PlayerReady, QJsonDocument(info).toJson(QJsonDocument::Compact));
}

void NetworkManager::sendGameStart()
{
    sendMessage(MessageType::GameStart);
}

void NetworkManager::sendPing()
{
    sendMessage(MessageType::Ping);
}

void NetworkManager::onSocketDisconnected()
{
    bool wasConnected = m_connected;
    m_connected = false;
    m_pingTimer->stop();
    
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    
    // Server continues accepting connections, we handle rejection in onNewConnection
    
    if (wasConnected) {
        emit opponentDisconnected();
        emit disconnected();
    }
}

void NetworkManager::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    
    QString errorString;
    if (m_socket) {
        errorString = m_socket->errorString();
    } else {
        errorString = tr("Unknown network error");
    }
    
    emit connectionError(errorString);
    
    if (!m_connected) {
        // Connection failed during initial connect
        if (m_socket) {
            m_socket->deleteLater();
            m_socket = nullptr;
        }
        m_role = NetworkRole::None;
    }
}
