#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QTcpSocket;
class QJsonObject;

class ChatClient : public QObject
{
    Q_OBJECT

public:
    explicit ChatClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port, const QString &userName);
    void disconnectFromServer();
    bool isConnected() const;
    QString userName() const;

public slots:
    void sendChat(const QString &text);
    void sendPrivate(const QString &to, const QString &text);

signals:
    void log(QString message);
    void connected();
    void disconnected();
    void loginOk(QString userName);
    void loginError(QString reason);
    void userListReceived(QStringList users);
    void chatReceived(QString from, QString text, bool isPrivate, QString to);
    void systemReceived(QString text);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(int socketError);

private:
    void sendJson(const QJsonObject &obj);
    void handleJson(const QJsonObject &obj);

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QString m_pendingUserName;
    QString m_userName;
    bool m_disconnectedNotified = true;
};
