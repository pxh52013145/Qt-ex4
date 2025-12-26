#include "serverwindow.h"
#include "ui_serverwindow.h"

#include "chatserver.h"

#include <QHostAddress>
#include <QMessageBox>

ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ServerWindow)
    , m_server(new ChatServer(this))
{
    ui->setupUi(this);

    connect(ui->pushButtonStartStop, &QPushButton::clicked, this, &ServerWindow::onStartStopClicked);
    connect(m_server, &ChatServer::log, this, &ServerWindow::onServerLog);
    connect(m_server, &ChatServer::usersChanged, this, &ServerWindow::onUsersChanged);
    connect(m_server, &ChatServer::runningChanged, this, &ServerWindow::onRunningChanged);

    setRunningUi(false);
}

ServerWindow::~ServerWindow()
{
    delete ui;
}

void ServerWindow::onStartStopClicked()
{
    if (m_server->isRunning()) {
        m_server->stop();
        return;
    }

    const auto port = static_cast<quint16>(ui->spinBoxPort->value());
    if (!m_server->start(QHostAddress::Any, port)) {
        QMessageBox::critical(this, tr("启动失败"), tr("监听端口失败：%1").arg(port));
    }
}

void ServerWindow::onServerLog(const QString &message)
{
    ui->plainTextEditLog->appendPlainText(message);
}

void ServerWindow::onUsersChanged(const QStringList &users)
{
    ui->labelOnline->setText(tr("在线人数：%1").arg(users.size()));
}

void ServerWindow::onRunningChanged(bool running)
{
    setRunningUi(running);
}

void ServerWindow::setRunningUi(bool running)
{
    ui->pushButtonStartStop->setText(running ? tr("停止服务器") : tr("启动服务器"));
    ui->spinBoxPort->setEnabled(!running);
}
