#include "clientwindow.h"
#include "ui_clientwindow.h"

#include "chatclient.h"
#include "protocol.h"

#include <QCloseEvent>
#include <QMessageBox>

ClientWindow::ClientWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ClientWindow)
    , m_client(new ChatClient(this))
{
    ui->setupUi(this);

    connect(ui->pushButtonLogin, &QPushButton::clicked, this, &ClientWindow::onLoginClicked);
    connect(ui->pushButtonSend, &QPushButton::clicked, this, &ClientWindow::onSendClicked);
    connect(ui->pushButtonExit, &QPushButton::clicked, this, &ClientWindow::onExitClicked);
    connect(ui->lineEditMessage, &QLineEdit::returnPressed, this, &ClientWindow::onSendClicked);
    connect(ui->lineEditHost, &QLineEdit::returnPressed, this, &ClientWindow::onLoginClicked);
    connect(ui->lineEditName, &QLineEdit::returnPressed, this, &ClientWindow::onLoginClicked);

    connect(m_client, &ChatClient::log, this, &ClientWindow::onClientLog);
    connect(m_client, &ChatClient::loginOk, this, &ClientWindow::onLoginOk);
    connect(m_client, &ChatClient::loginError, this, &ClientWindow::onLoginError);
    connect(m_client, &ChatClient::disconnected, this, &ClientWindow::onDisconnected);
    connect(m_client, &ChatClient::userListReceived, this, &ClientWindow::onUserListReceived);
    connect(m_client, &ChatClient::chatReceived, this, &ClientWindow::onChatReceived);
    connect(m_client, &ChatClient::systemReceived, this, &ClientWindow::onSystemReceived);

    ui->splitterChat->setStretchFactor(0, 4);
    ui->splitterChat->setStretchFactor(1, 1);
    ui->splitterChat->setSizes({720, 180});

    showLoginPage();
}

ClientWindow::~ClientWindow()
{
    delete ui;
}

void ClientWindow::onLoginClicked()
{
    if (m_client->isConnected()) {
        return;
    }

    const QString host = ui->lineEditHost->text().trimmed();
    const QString name = ui->lineEditName->text().trimmed();

    if (host.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("服务器IP地址不能为空"));
        return;
    }
    if (!Protocol::isValidName(Protocol::normalizeName(name))) {
        QMessageBox::warning(this, tr("输入错误"), tr("昵称不能为空，且长度不能超过 %1").arg(Protocol::kMaxNameLength));
        return;
    }

    setLoginEnabled(false);
    statusBar()->showMessage(tr("正在连接 %1:%2 ...").arg(host).arg(Protocol::kDefaultPort), 5000);
    m_client->connectToServer(host, Protocol::kDefaultPort, name);
}

void ClientWindow::onSendClicked()
{
    const QString text = ui->lineEditMessage->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    QString to;
    QString message;

    if (text.startsWith("/w ") || text.startsWith("/msg ")) {
        const QStringList parts = text.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            to = parts.at(1);
            message = text.section(' ', 2);
        }
    } else if (text.startsWith('@')) {
        const int space = text.indexOf(' ');
        if (space > 1) {
            to = text.mid(1, space - 1);
            message = text.mid(space + 1).trimmed();
        }
    }

    if (!to.isEmpty() && !message.isEmpty()) {
        m_client->sendPrivate(to, message);
    } else {
        m_client->sendChat(text);
    }

    ui->lineEditMessage->clear();
}

void ClientWindow::onExitClicked()
{
    const auto choice = QMessageBox::question(this,
        tr("确认退出"),
        tr("确定要退出聊天室并断开连接吗？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (choice != QMessageBox::Yes) {
        return;
    }

    m_client->disconnectFromServer();
    showLoginPage();
}

void ClientWindow::closeEvent(QCloseEvent *event)
{
    if (!m_client->isConnected()) {
        QMainWindow::closeEvent(event);
        return;
    }

    const auto choice = QMessageBox::question(this,
        tr("确认退出"),
        tr("退出程序会断开连接，确定要退出吗？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (choice != QMessageBox::Yes) {
        event->ignore();
        return;
    }

    m_client->disconnectFromServer();
    QMainWindow::closeEvent(event);
}

void ClientWindow::onClientLog(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}

void ClientWindow::onLoginOk(const QString &userName)
{
    ui->labelTitle->setText(tr("%1 的聊天室（实验4）").arg(userName));
    statusBar()->showMessage(tr("登录成功：%1").arg(userName), 5000);
    showChatPage();
}

void ClientWindow::onLoginError(const QString &reason)
{
    QMessageBox::critical(this, tr("登录失败"), tr("登录失败：%1").arg(reason));
    showLoginPage();
}

void ClientWindow::onDisconnected()
{
    statusBar()->showMessage(tr("已断开连接"), 5000);
    showLoginPage();
}

void ClientWindow::onUserListReceived(const QStringList &users)
{
    ui->listWidgetUsers->clear();
    ui->listWidgetUsers->addItems(users);
}

void ClientWindow::onChatReceived(const QString &from, const QString &text, bool isPrivate, const QString &to)
{
    if (isPrivate && !to.isEmpty()) {
        appendChatLine(QString("%1 -> %2 : %3").arg(from, to, text));
        return;
    }
    appendChatLine(QString("%1 : %2").arg(from, text));
}

void ClientWindow::onSystemReceived(const QString &text)
{
    appendChatLine(tr("系统 : %1").arg(text));
}

void ClientWindow::setLoginEnabled(bool enabled)
{
    ui->lineEditHost->setEnabled(enabled);
    ui->lineEditName->setEnabled(enabled);
    ui->pushButtonLogin->setEnabled(enabled);
}

void ClientWindow::showLoginPage()
{
    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    setLoginEnabled(true);
    ui->plainTextEditChat->clear();
    ui->listWidgetUsers->clear();
    ui->lineEditMessage->clear();
    ui->lineEditName->setFocus();
}

void ClientWindow::showChatPage()
{
    ui->stackedWidget->setCurrentWidget(ui->pageChat);
    ui->lineEditMessage->setFocus();
}

void ClientWindow::appendChatLine(const QString &line)
{
    ui->plainTextEditChat->appendPlainText(line);
}
