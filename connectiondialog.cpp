#include "connectiondialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

ConnectionDialog::ConnectionDialog(NetworkManager* networkManager, QWidget *parent)
    : QDialog(parent)
    , m_networkManager(networkManager)
{
    setWindowTitle(tr("Connect to Game"));
    setMinimumSize(450, 400);
    
    setupUI();
    startDiscovery();
}

void ConnectionDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    m_tabWidget = new QTabWidget(this);
    
    // ==================== Host Tab ====================
    QWidget* hostTab = new QWidget();
    QVBoxLayout* hostLayout = new QVBoxLayout(hostTab);
    
    // Player info
    QGroupBox* hostInfoGroup = new QGroupBox(tr("Your Information"));
    QFormLayout* hostInfoLayout = new QFormLayout(hostInfoGroup);
    
    m_hostNameEdit = new QLineEdit();
    m_hostNameEdit->setPlaceholderText(tr("Enter your name"));
    m_hostNameEdit->setText(tr("Player 1"));
    hostInfoLayout->addRow(tr("Name:"), m_hostNameEdit);
    
    m_hostPortSpinBox = new QSpinBox();
    m_hostPortSpinBox->setRange(1024, 65535);
    m_hostPortSpinBox->setValue(NetworkManager::DEFAULT_PORT);
    hostInfoLayout->addRow(tr("Port:"), m_hostPortSpinBox);
    
    m_localIPLabel = new QLabel(NetworkManager::getLocalIPAddress());
    m_localIPLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hostInfoLayout->addRow(tr("Your IP:"), m_localIPLabel);
    
    hostLayout->addWidget(hostInfoGroup);
    
    // Instructions
    QLabel* hostInstructions = new QLabel(
        tr("Start hosting a game. Other players on your network can find and join your game.\n\n"
           "Share your IP address with players who can't see your game in the list."));
    hostInstructions->setWordWrap(true);
    hostInstructions->setStyleSheet("color: gray; padding: 10px;");
    hostLayout->addWidget(hostInstructions);
    
    hostLayout->addStretch();
    
    m_hostButton = new QPushButton(tr("Host Game"));
    m_hostButton->setMinimumHeight(40);
    m_hostButton->setStyleSheet("QPushButton { font-weight: bold; }");
    connect(m_hostButton, &QPushButton::clicked, this, &ConnectionDialog::onHostClicked);
    hostLayout->addWidget(m_hostButton);
    
    m_tabWidget->addTab(hostTab, tr("Host Game"));
    
    // ==================== Join Tab ====================
    QWidget* joinTab = new QWidget();
    QVBoxLayout* joinLayout = new QVBoxLayout(joinTab);
    
    // Player info
    QGroupBox* joinInfoGroup = new QGroupBox(tr("Your Information"));
    QFormLayout* joinInfoLayout = new QFormLayout(joinInfoGroup);
    
    m_joinNameEdit = new QLineEdit();
    m_joinNameEdit->setPlaceholderText(tr("Enter your name"));
    m_joinNameEdit->setText(tr("Player 2"));
    joinInfoLayout->addRow(tr("Name:"), m_joinNameEdit);
    
    joinLayout->addWidget(joinInfoGroup);
    
    // Available games
    QGroupBox* gamesGroup = new QGroupBox(tr("Available Games on Network"));
    QVBoxLayout* gamesLayout = new QVBoxLayout(gamesGroup);
    
    m_peerList = new QListWidget();
    m_peerList->setMinimumHeight(100);
    connect(m_peerList, &QListWidget::itemClicked, this, &ConnectionDialog::onPeerSelected);
    connect(m_peerList, &QListWidget::itemDoubleClicked, this, &ConnectionDialog::onPeerDoubleClicked);
    gamesLayout->addWidget(m_peerList);
    
    QHBoxLayout* refreshLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Searching for games..."));
    m_statusLabel->setStyleSheet("color: gray;");
    refreshLayout->addWidget(m_statusLabel);
    refreshLayout->addStretch();
    
    m_refreshButton = new QPushButton(tr("Refresh"));
    connect(m_refreshButton, &QPushButton::clicked, this, &ConnectionDialog::refreshPeerList);
    refreshLayout->addWidget(m_refreshButton);
    
    gamesLayout->addLayout(refreshLayout);
    joinLayout->addWidget(gamesGroup);
    
    // Manual connection
    QGroupBox* manualGroup = new QGroupBox(tr("Manual Connection"));
    QHBoxLayout* manualLayout = new QHBoxLayout(manualGroup);
    
    m_manualHostEdit = new QLineEdit();
    m_manualHostEdit->setPlaceholderText(tr("IP Address (e.g., 192.168.1.100)"));
    connect(m_manualHostEdit, &QLineEdit::textChanged, this, &ConnectionDialog::updateJoinButtonState);
    manualLayout->addWidget(m_manualHostEdit, 3);
    
    m_joinPortSpinBox = new QSpinBox();
    m_joinPortSpinBox->setRange(1024, 65535);
    m_joinPortSpinBox->setValue(NetworkManager::DEFAULT_PORT);
    manualLayout->addWidget(m_joinPortSpinBox, 1);
    
    joinLayout->addWidget(manualGroup);
    
    m_joinButton = new QPushButton(tr("Join Game"));
    m_joinButton->setMinimumHeight(40);
    m_joinButton->setStyleSheet("QPushButton { font-weight: bold; }");
    m_joinButton->setEnabled(false);
    connect(m_joinButton, &QPushButton::clicked, this, &ConnectionDialog::onJoinClicked);
    joinLayout->addWidget(m_joinButton);
    
    m_tabWidget->addTab(joinTab, tr("Join Game"));
    
    mainLayout->addWidget(m_tabWidget);
    
    // Cancel button
    QPushButton* cancelButton = new QPushButton(tr("Cancel"));
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    mainLayout->addWidget(cancelButton);
    
    // Connect to network manager signals
    connect(m_networkManager, &NetworkManager::peersChanged, this, &ConnectionDialog::refreshPeerList);
}

void ConnectionDialog::startDiscovery()
{
    m_networkManager->startDiscovery();
    refreshPeerList();
}

void ConnectionDialog::stopDiscovery()
{
    m_networkManager->stopDiscovery();
}

void ConnectionDialog::refreshPeerList()
{
    m_peerList->clear();
    
    QList<PeerInfo> peers = m_networkManager->discoveredPeers();
    
    for (const PeerInfo& peer : peers) {
        QString text = QString("%1 (%2:%3)")
                           .arg(peer.name)
                           .arg(peer.address.toString())
                           .arg(peer.port);
        
        QListWidgetItem* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, peer.address.toString());
        item->setData(Qt::UserRole + 1, peer.port);
        m_peerList->addItem(item);
    }
    
    if (peers.isEmpty()) {
        m_statusLabel->setText(tr("No games found. Searching..."));
    } else {
        m_statusLabel->setText(tr("%1 game(s) found").arg(peers.size()));
    }
}

void ConnectionDialog::onPeerSelected(QListWidgetItem* item)
{
    if (!item) return;
    
    m_manualHostEdit->setText(item->data(Qt::UserRole).toString());
    m_joinPortSpinBox->setValue(item->data(Qt::UserRole + 1).toInt());
    updateJoinButtonState();
}

void ConnectionDialog::onPeerDoubleClicked(QListWidgetItem* item)
{
    onPeerSelected(item);
    onJoinClicked();
}

void ConnectionDialog::updateJoinButtonState()
{
    bool hasHost = !m_manualHostEdit->text().trimmed().isEmpty();
    bool hasName = !m_joinNameEdit->text().trimmed().isEmpty();
    m_joinButton->setEnabled(hasHost && hasName);
}

void ConnectionDialog::onHostClicked()
{
    QString name = m_hostNameEdit->text().trimmed();
    
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Information"), 
                            tr("Please enter your name."));
        m_hostNameEdit->setFocus();
        return;
    }
    
    stopDiscovery();
    m_result = Result::Host;
    emit hostRequested(name, m_hostPortSpinBox->value());
    accept();
}

void ConnectionDialog::onJoinClicked()
{
    QString name = m_joinNameEdit->text().trimmed();
    QString host = m_manualHostEdit->text().trimmed();
    
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Information"), 
                            tr("Please enter your name."));
        m_joinNameEdit->setFocus();
        return;
    }
    
    if (host.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Information"), 
                            tr("Please enter a host IP address or select a game from the list."));
        m_manualHostEdit->setFocus();
        return;
    }
    
    stopDiscovery();
    m_result = Result::Join;
    emit joinRequested(name, host, m_joinPortSpinBox->value());
    accept();
}

QString ConnectionDialog::playerName() const
{
    if (m_result == Result::Host) {
        return m_hostNameEdit->text().trimmed();
    } else {
        return m_joinNameEdit->text().trimmed();
    }
}

QString ConnectionDialog::hostAddress() const
{
    return m_manualHostEdit->text().trimmed();
}

quint16 ConnectionDialog::port() const
{
    if (m_result == Result::Host) {
        return m_hostPortSpinBox->value();
    } else {
        return m_joinPortSpinBox->value();
    }
}
