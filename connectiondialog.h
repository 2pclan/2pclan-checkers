#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QTabWidget>
#include <QSpinBox>
#include "networkmanager.h"

class ConnectionDialog : public QDialog
{
    Q_OBJECT
    
public:
    enum class Result {
        Cancelled,
        Host,
        Join
    };
    
    explicit ConnectionDialog(NetworkManager* networkManager, QWidget *parent = nullptr);
    
    Result connectionResult() const { return m_result; }
    QString playerName() const;
    QString hostAddress() const;
    quint16 port() const;
    
signals:
    void hostRequested(const QString& playerName, quint16 port);
    void joinRequested(const QString& playerName, const QString& host, quint16 port);
    
private slots:
    void onHostClicked();
    void onJoinClicked();
    void onPeerSelected(QListWidgetItem* item);
    void onPeerDoubleClicked(QListWidgetItem* item);
    void refreshPeerList();
    void updateJoinButtonState();
    
private:
    void setupUI();
    void startDiscovery();
    void stopDiscovery();
    
    NetworkManager* m_networkManager;
    Result m_result = Result::Cancelled;
    
    // UI Elements
    QTabWidget* m_tabWidget;
    
    // Host tab
    QLineEdit* m_hostNameEdit;
    QSpinBox* m_hostPortSpinBox;
    QLabel* m_localIPLabel;
    QPushButton* m_hostButton;
    
    // Join tab
    QLineEdit* m_joinNameEdit;
    QListWidget* m_peerList;
    QLineEdit* m_manualHostEdit;
    QSpinBox* m_joinPortSpinBox;
    QPushButton* m_joinButton;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
};

#endif // CONNECTIONDIALOG_H
