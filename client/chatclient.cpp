#include "chatclient.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::onDisconnected);
    connect(m_socket,
        QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
        this,
        &ChatClient::onError);
}

void ChatClient::connectToServer(const QString &host, quint16 port, const QString &userName)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_disconnectedNotified = false;
    m_pendingUserName = Protocol::normalizeName(userName);
    m_userName.clear();
    m_buffer.clear();

    emit log(QString("connecting to %1:%2...").arg(host).arg(port));
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }
    sendJson(QJsonObject{{"type", "logout"}});
    m_socket->disconnectFromHost();
}

bool ChatClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QString ChatClient::userName() const
{
    return m_userName;
}

void ChatClient::sendChat(const QString &text)
{
    const QString normalized = Protocol::normalizeText(text);
    if (!isConnected() || !Protocol::isValidMessage(normalized)) {
        return;
    }
    sendJson(QJsonObject{{"type", "chat"}, {"text", normalized}});
}

void ChatClient::sendPrivate(const QString &to, const QString &text)
{
    const QString normalizedTo = Protocol::normalizeName(to);
    const QString normalizedText = Protocol::normalizeText(text);
    if (!isConnected() || !Protocol::isValidName(normalizedTo) || !Protocol::isValidMessage(normalizedText)) {
        return;
    }
    sendJson(QJsonObject{{"type", "private"}, {"to", normalizedTo}, {"text", normalizedText}});
}

void ChatClient::onConnected()
{
    emit log("tcp connected, sending login...");
    emit connected();
    sendJson(QJsonObject{{"type", "login"}, {"name", m_pendingUserName}});
}

void ChatClient::onReadyRead()
{
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

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit log(QString("invalid json from server: %1").arg(err.errorString()));
            continue;
        }

        handleJson(doc.object());
    }
}

void ChatClient::onDisconnected()
{
    if (m_disconnectedNotified) {
        return;
    }

    m_disconnectedNotified = true;
    emit log("disconnected");
    m_userName.clear();
    emit disconnected();
}

void ChatClient::onError(int)
{
    emit log(QString("socket error: %1").arg(m_socket->errorString()));
    if (!m_disconnectedNotified && m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_disconnectedNotified = true;
        m_userName.clear();
        emit disconnected();
    }
}

void ChatClient::sendJson(const QJsonObject &obj)
{
    m_socket->write(Protocol::toLine(obj));
}

void ChatClient::handleJson(const QJsonObject &obj)
{
    const QString type = obj.value("type").toString();
    if (type == "login_ok") {
        m_userName = obj.value("name").toString();
        emit log(QString("login ok: %1").arg(m_userName));
        emit loginOk(m_userName);
        return;
    }

    if (type == "login_error") {
        const QString reason = obj.value("reason").toString();
        emit log(QString("login error: %1").arg(reason));
        emit loginError(reason);
        m_socket->disconnectFromHost();
        return;
    }

    if (type == "user_list") {
        QStringList users;
        const QJsonArray arr = obj.value("users").toArray();
        users.reserve(arr.size());
        for (const auto &v : arr) {
            users.push_back(v.toString());
        }
        emit userListReceived(users);
        return;
    }

    if (type == "chat") {
        const QString from = obj.value("from").toString();
        const QString text = obj.value("text").toString();
        const bool isPrivate = obj.value("scope").toString() == "private";
        const QString to = obj.value("to").toString();
        emit chatReceived(from, text, isPrivate, to);
        return;
    }

    if (type == "system") {
        emit systemReceived(obj.value("text").toString());
        return;
    }

    if (type == "error") {
        emit systemReceived(obj.value("message").toString());
        return;
    }
}
