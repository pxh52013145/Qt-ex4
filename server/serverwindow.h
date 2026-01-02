#pragma once

#include <QMainWindow>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui {
class ServerWindow;
}
QT_END_NAMESPACE

class ChatServer;

class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ServerWindow(QWidget *parent = nullptr);
    ~ServerWindow() override;

private slots:
    void onStartStopClicked();
    void onServerLog(const QString &message);
    void onUsersChanged(const QStringList &users);
    void onRunningChanged(bool running);

private:
    void setRunningUi(bool running);

    Ui::ServerWindow *ui = nullptr;
    ChatServer *m_server = nullptr;
};
