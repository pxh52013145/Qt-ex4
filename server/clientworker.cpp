#include "clientworker.h"

#include <QTcpSocket>

ClientWorker::ClientWorker(quint64 clientId, qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_clientId(clientId)
    , m_socketDescriptor(socketDescriptor)
{
}

void ClientWorker::start()
{
    if (m_socket) {
        return;
    }

    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(m_socketDescriptor)) {
        emit log(m_clientId, QString("setSocketDescriptor failed: %1").arg(m_socket->errorString()));
        emit disconnected(m_clientId);
        m_socket->deleteLater();
        m_socket = nullptr;
        return;
    }

    connect(m_socket, &QTcpSocket::readyRead, this, &ClientWorker::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientWorker::onDisconnected);
    connect(m_socket,
        QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
        this,
        &ClientWorker::onError);

    emit log(m_clientId, "client socket ready");
}

void ClientWorker::sendLine(QByteArray line)
{
    if (!m_socket) {
        return;
    }
    if (!line.endsWith('\n')) {
        line.append('\n');
    }
    m_socket->write(line);
}

void ClientWorker::disconnectFromHost()
{
    if (!m_socket) {
        return;
    }
    m_socket->disconnectFromHost();
}

void ClientWorker::onReadyRead()
{
    if (!m_socket) {
        return;
    }

    m_buffer.append(m_socket->readAll());

    while (true) {
        const int newlineIndex = m_buffer.indexOf('\n');
        if (newlineIndex < 0) {
            break;
        }

        QByteArray line = m_buffer.left(newlineIndex);
        m_buffer.remove(0, newlineIndex + 1);

        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        emit lineReceived(m_clientId, line);
    }
}

void ClientWorker::onDisconnected()
{
    emit log(m_clientId, "client disconnected");
    emit disconnected(m_clientId);
}

void ClientWorker::onError(int)
{
    if (!m_socket) {
        return;
    }
    emit log(m_clientId, QString("socket error: %1").arg(m_socket->errorString()));
}

