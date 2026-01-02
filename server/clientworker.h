#pragma once

#include <QByteArray>
#include <QObject>

class QTcpSocket;

class ClientWorker : public QObject
{
    Q_OBJECT

public:
    explicit ClientWorker(quint64 clientId, qintptr socketDescriptor, QObject *parent = nullptr);

signals:
    void lineReceived(quint64 clientId, QByteArray line);
    void disconnected(quint64 clientId);
    void log(quint64 clientId, QString message);

public slots:
    void start();
    void sendLine(QByteArray line);
    void disconnectFromHost();

private slots:
    void onReadyRead();
    void onDisconnected();
    void onError(int socketError);

private:
    const quint64 m_clientId;
    const qintptr m_socketDescriptor;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
};

