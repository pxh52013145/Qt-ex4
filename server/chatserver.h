#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QSemaphore>
#include <QString>
#include <QStringList>

class ClientWorker;
class QThread;
class QTcpServer;
class ThreadedTcpServer;

class ChatServer : public QObject
{
    Q_OBJECT
    friend class ThreadedTcpServer;

public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer() override;

    bool start(const QHostAddress &address, quint16 port);
    void stop();
    bool isRunning() const;

signals:
    void log(QString message);
    void runningChanged(bool running);
    void usersChanged(QStringList users);

private slots:
    void onClientLine(quint64 clientId, QByteArray line);
    void onClientDisconnected(quint64 clientId);

private:
    struct ClientEntry {
        QString name;
        ClientWorker *worker = nullptr;
        QThread *thread = nullptr;
        bool loggedIn = false;
    };

    void onIncomingConnection(qintptr socketDescriptor);
    void removeClient(quint64 clientId, bool announce);
    void sendJson(quint64 clientId, const QJsonObject &obj);
    void broadcastJson(const QJsonObject &obj, quint64 exceptClientId = 0);
    void broadcastUsers();
    QStringList currentUsers() const;

    QTcpServer *m_server = nullptr;
    quint64 m_nextClientId = 1;
    bool m_stopping = false;
    QSemaphore m_connectionLimit;

    QHash<quint64, ClientEntry> m_clients;
    QHash<QString, quint64> m_nameToId;
};
