#include "chatserver.h"

#include "clientworker.h"
#include "protocol.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

class ThreadedTcpServer final : public QTcpServer
{
public:
    explicit ThreadedTcpServer(ChatServer *owner)
        : QTcpServer(owner)
        , m_owner(owner)
    {
    }

protected:
    void incomingConnection(qintptr socketDescriptor) override
    {
        m_owner->onIncomingConnection(socketDescriptor);
    }

private:
    ChatServer *m_owner = nullptr;
};

static QString toCompactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

static QString toPrettyJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)).trimmed();
}

static QJsonObject systemMessage(const QString &text)
{
    return QJsonObject{
        {"type", "system"},
        {"text", text},
        {"time", QDateTime::currentDateTime().toString(Qt::ISODate)},
    };
}

ChatServer::ChatServer(QObject *parent)
    : QObject(parent)
    , m_server(new ThreadedTcpServer(this))
    , m_connectionLimit(100)
{
}

ChatServer::~ChatServer()
{
    stop();
}

bool ChatServer::start(const QHostAddress &address, quint16 port)
{
    stop();

    const bool ok = m_server->listen(address, port);
    if (ok) {
        emit log(QString("listening on %1:%2").arg(m_server->serverAddress().toString()).arg(m_server->serverPort()));
        emit runningChanged(true);
    } else {
        emit log(QString("listen failed: %1").arg(m_server->errorString()));
        emit runningChanged(false);
    }
    return ok;
}

void ChatServer::stop()
{
    if (!isRunning() && m_clients.isEmpty()) {
        return;
    }

    m_stopping = true;
    m_server->close();

    const auto clientIds = m_clients.keys();
    for (quint64 id : clientIds) {
        removeClient(id, false);
    }

    m_stopping = false;
    emit usersChanged({});
    emit runningChanged(false);
    emit log("server stopped");
}

bool ChatServer::isRunning() const
{
    return m_server->isListening();
}

void ChatServer::onIncomingConnection(qintptr socketDescriptor)
{
    if (!m_connectionLimit.tryAcquire(1)) {
        emit log("connection rejected: too many clients");
        auto *socket = new QTcpSocket(this);
        if (socket->setSocketDescriptor(socketDescriptor)) {
            socket->disconnectFromHost();
        }
        socket->deleteLater();
        return;
    }

    const quint64 clientId = m_nextClientId++;

    auto *thread = new QThread;
    thread->setObjectName(QStringLiteral("client-%1").arg(clientId));

    auto *worker = new ClientWorker(clientId, socketDescriptor);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &ClientWorker::start);
    connect(worker, &ClientWorker::lineReceived, this, &ChatServer::onClientLine);
    connect(worker, &ClientWorker::disconnected, this, &ChatServer::onClientDisconnected);
    connect(worker, &ClientWorker::disconnected, thread, &QThread::quit);
    connect(worker, &ClientWorker::log, this, [this](quint64 id, const QString &msg) { emit log(QString("[%1] %2").arg(id).arg(msg)); });

    ClientEntry entry;
    entry.worker = worker;
    entry.thread = thread;
    m_clients.insert(clientId, entry);

    emit log(QString("[%1] incoming connection").arg(clientId));
    thread->start();
}

void ChatServer::onClientLine(quint64 clientId, QByteArray line)
{
    const auto it = m_clients.find(clientId);
    if (it == m_clients.end()) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit log(QString("[%1] invalid json: %2").arg(clientId).arg(err.errorString()));
        sendJson(clientId, QJsonObject{{"type", "error"}, {"message", "invalid json"}});
        return;
    }

    const QJsonObject obj = doc.object();
    {
        const auto &client = m_clients[clientId];
        const QString who = (client.loggedIn && !client.name.isEmpty()) ? client.name : QString("#%1").arg(clientId);
        emit log(QString("[%1] JSON received from %2:\n%3").arg(clientId).arg(who, toPrettyJson(obj)));
    }

    const QString type = obj.value("type").toString();
    if (type.isEmpty()) {
        sendJson(clientId, QJsonObject{{"type", "error"}, {"message", "missing type"}});
        return;
    }

    auto &client = m_clients[clientId];

    if (type == "login") {
        if (client.loggedIn) {
            sendJson(clientId, QJsonObject{{"type", "login_error"}, {"reason", "already_logged_in"}});
            return;
        }

        QString name = Protocol::normalizeName(obj.value("name").toString());
        if (!Protocol::isValidName(name)) {
            sendJson(clientId, QJsonObject{{"type", "login_error"}, {"reason", "invalid_name"}});
            if (client.worker) {
                QMetaObject::invokeMethod(client.worker, "disconnectFromHost", Qt::QueuedConnection);
            }
            return;
        }

        if (m_nameToId.contains(name)) {
            sendJson(clientId, QJsonObject{{"type", "login_error"}, {"reason", "name_taken"}});
            if (client.worker) {
                QMetaObject::invokeMethod(client.worker, "disconnectFromHost", Qt::QueuedConnection);
            }
            return;
        }

        client.name = name;
        client.loggedIn = true;
        m_nameToId.insert(name, clientId);

        sendJson(clientId, QJsonObject{{"type", "login_ok"}, {"name", name}});
        broadcastJson(systemMessage(QString("%1 joined").arg(name)));
        broadcastUsers();
        emit log(QString("[%1] login ok: %2").arg(clientId).arg(name));
        return;
    }

    if (!client.loggedIn) {
        sendJson(clientId, QJsonObject{{"type", "error"}, {"message", "not logged in"}});
        return;
    }

    if (type == "chat") {
        const QString text = Protocol::normalizeText(obj.value("text").toString());
        if (!Protocol::isValidMessage(text)) {
            sendJson(clientId, QJsonObject{{"type", "error"}, {"message", "invalid message"}});
            return;
        }

        const QJsonObject msg{
            {"type", "chat"},
            {"scope", "broadcast"},
            {"from", client.name},
            {"text", text},
            {"time", QDateTime::currentDateTime().toString(Qt::ISODate)},
        };
        broadcastJson(msg);
        emit log(QString("[%1] %2: %3").arg(clientId).arg(client.name, text));
        return;
    }

    if (type == "private") {
        const QString to = Protocol::normalizeName(obj.value("to").toString());
        const QString text = Protocol::normalizeText(obj.value("text").toString());
        if (!Protocol::isValidName(to) || !Protocol::isValidMessage(text)) {
            sendJson(clientId, QJsonObject{{"type", "error"}, {"message", "invalid private message"}});
            return;
        }

        const auto destIt = m_nameToId.find(to);
        if (destIt == m_nameToId.end()) {
            sendJson(clientId, systemMessage(QString("user not found: %1").arg(to)));
            return;
        }

        const QJsonObject msg{
            {"type", "chat"},
            {"scope", "private"},
            {"from", client.name},
            {"to", to},
            {"text", text},
            {"time", QDateTime::currentDateTime().toString(Qt::ISODate)},
        };

        sendJson(*destIt, msg);
        sendJson(clientId, msg);
        emit log(QString("[%1] %2 -> %3: %4").arg(clientId).arg(client.name, to, text));
        return;
    }

    if (type == "logout") {
        if (client.worker) {
            QMetaObject::invokeMethod(client.worker, "disconnectFromHost", Qt::QueuedConnection);
        }
        return;
    }

    sendJson(clientId, QJsonObject{{"type", "error"}, {"message", "unknown type"}});
}

void ChatServer::onClientDisconnected(quint64 clientId)
{
    removeClient(clientId, !m_stopping);
}

void ChatServer::removeClient(quint64 clientId, bool announce)
{
    const auto it = m_clients.find(clientId);
    if (it == m_clients.end()) {
        return;
    }

    const ClientEntry entry = it.value();
    m_clients.erase(it);

    if (entry.loggedIn) {
        m_nameToId.remove(entry.name);
        if (announce) {
            broadcastJson(systemMessage(QString("%1 left").arg(entry.name)));
            broadcastUsers();
        }
    }

    if (entry.worker) {
        QMetaObject::invokeMethod(entry.worker, "disconnectFromHost", Qt::QueuedConnection);
    }

    if (entry.thread) {
        entry.thread->quit();
        if (!entry.thread->wait(2000)) {
            emit log(QString("[%1] thread quit timeout, terminating").arg(clientId));
            entry.thread->terminate();
            entry.thread->wait(1000);
        }
    }

    if (entry.worker) {
        entry.worker->moveToThread(QThread::currentThread());
        delete entry.worker;
    }
    if (entry.thread) {
        delete entry.thread;
    }

    m_connectionLimit.release(1);
}

void ChatServer::sendJson(quint64 clientId, const QJsonObject &obj)
{
    const auto it = m_clients.find(clientId);
    if (it == m_clients.end() || !it.value().worker) {
        return;
    }

    const QString to = (it.value().loggedIn && !it.value().name.isEmpty()) ? it.value().name : QString("#%1").arg(clientId);
    emit log(QString("Sending to %1 - %2").arg(to, toCompactJson(obj)));

    const QByteArray line = Protocol::toLine(obj);
    QMetaObject::invokeMethod(it.value().worker, "sendLine", Qt::QueuedConnection, Q_ARG(QByteArray, line));
}

void ChatServer::broadcastJson(const QJsonObject &obj, quint64 exceptClientId)
{
    const QByteArray line = Protocol::toLine(obj);
    const QString compact = toCompactJson(obj);
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        const auto clientId = it.key();
        const auto &client = it.value();
        if (!client.loggedIn || !client.worker) {
            continue;
        }
        if (exceptClientId != 0 && clientId == exceptClientId) {
            continue;
        }
        emit log(QString("Sending to %1 - %2").arg(client.name, compact));
        QMetaObject::invokeMethod(client.worker, "sendLine", Qt::QueuedConnection, Q_ARG(QByteArray, line));
    }
}

void ChatServer::broadcastUsers()
{
    const QStringList users = currentUsers();
    emit usersChanged(users);

    QJsonArray arr;
    for (const auto &u : users) {
        arr.append(u);
    }
    broadcastJson(QJsonObject{{"type", "user_list"}, {"users", arr}});
}

QStringList ChatServer::currentUsers() const
{
    QStringList users;
    users.reserve(m_clients.size());
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        if (it.value().loggedIn) {
            users.push_back(it.value().name);
        }
    }
    users.sort(Qt::CaseInsensitive);
    return users;
}
