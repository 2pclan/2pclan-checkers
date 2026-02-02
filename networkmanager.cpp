#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QIODevice>

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
    
    // Collect local IP addresses for filtering
    updateLocalAddresses();
}

NetworkManager::~NetworkManager()
{
    disconnect();
}

void NetworkManager::updateLocalAddresses()
{
    m_localAddresses.clear();
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            m_localAddresses.insert(entry.ip().toString());
        }
    }
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
    
    // Announce immediately multiple times to ensure visibility
    announcePresence();
    QTimer::singleShot(500, this, &NetworkManager::announcePresence);
    QTimer::singleShot(1000, this, &NetworkManager::announcePresence);
    
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
    // Close if already open
    if (m_discoverySocket->state() != QAbstractSocket::UnconnectedState) {
        m_discoverySocket->close();
    }
    
    // Bind to discovery port with sharing enabled
    if (!m_discoverySocket->bind(QHostAddress::Any, DISCOVERY_PORT, 
                                  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning() << "Failed to bind discovery socket:" << m_discoverySocket->errorString();
        // Try without specific port
        m_discoverySocket->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress);
    }
    
    m_discoveryTimer->start(DISCOVERY_INTERVAL_MS);
    m_cleanupTimer->start(1000);
    
    // Update local addresses for filtering
    updateLocalAddresses();
    
    // Announce immediately if hosting
    if (m_role == NetworkRole::Host) {
        announcePresence();
    }
}

void NetworkManager::stopDiscovery()
{
    m_discoveryTimer->stop();
    m_cleanupTimer->stop();
    if (m_discoverySocket->state() != QAbstractSocket::UnconnectedState) {
        m_discoverySocket->close();
    }
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
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    
    // Broadcast on all interfaces to ensure discovery works
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp) ||
            !iface.flags().testFlag(QNetworkInterface::IsRunning) ||
            iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            
            QHostAddress broadcast = entry.broadcast();
            if (!broadcast.isNull()) {
                m_discoverySocket->writeDatagram(data, broadcast, DISCOVERY_PORT);
            }
        }
    }
    
    // Also send to general broadcast address
    m_discoverySocket->writeDatagram(data, QHostAddress::Broadcast, DISCOVERY_PORT);
}

void NetworkManager::onDiscoveryReadyRead()
{
    while (m_discoverySocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_discoverySocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        
        m_discoverySocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        // Convert to IPv4 if it's an IPv4-mapped IPv6 address
        if (sender.protocol() == QAbstractSocket::IPv6Protocol) {
            bool ok;
            QHostAddress ipv4 = QHostAddress(sender.toIPv4Address(&ok));
            if (ok) {
                sender = ipv4;
            }
        }
        
        // Ignore our own broadcasts
        if (m_localAddresses.contains(sender.toString())) {
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
    
    // Start keep-alive
    m_pingTimer->start(5000);
    
    // Stop announcing since we have a player
    m_discoveryTimer->stop();
    
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
    
    // Resume announcing if still hosting
    if (m_role == NetworkRole::Host && m_server->isListening()) {
        m_discoveryTimer->start(DISCOVERY_INTERVAL_MS);
        announcePresence();
    }
    
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
