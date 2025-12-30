#pragma once

#include <QCloseEvent>
#include <QMainWindow>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui {
class ClientWindow;
}
QT_END_NAMESPACE

class ChatClient;

class ClientWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientWindow(QWidget *parent = nullptr);
    ~ClientWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLoginClicked();
    void onSendClicked();
    void onExitClicked();
    void onClientLog(const QString &message);
    void onLoginOk(const QString &userName);
    void onLoginError(const QString &reason);
    void onDisconnected();
    void onUserListReceived(const QStringList &users);
    void onChatReceived(const QString &from, const QString &text, bool isPrivate, const QString &to);
    void onSystemReceived(const QString &text);

private:
    void setLoginEnabled(bool enabled);
    void showLoginPage();
    void showChatPage();
    void appendChatLine(const QString &line);

    Ui::ClientWindow *ui = nullptr;
    ChatClient *m_client = nullptr;
};
