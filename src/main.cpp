#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QUrl>
#include <QTimer>
#include <QLocalSocket>
#include <QLocalServer>

#include <X11/Xlib.h>

int main(int argc, char* argv[])
{
    XInitThreads();

    QApplication::setApplicationName(QStringLiteral("Omni Clicker"));
    QApplication::setApplicationDisplayName(QStringLiteral("Omni Clicker"));
    QApplication::setOrganizationName(QStringLiteral("omniclicker"));
    QApplication::setDesktopFileName(QStringLiteral("io.github.omniclicker"));

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Native Linux autoclicker for X11 and Wayland-aware sessions."));
    parser.addHelpOption();
    QCommandLineOption minimizedOption(QStringLiteral("start-minimized"), QStringLiteral("Start minimized."));
    parser.addOption(minimizedOption);
    QCommandLineOption toggleOption(QStringLiteral("toggle"), QStringLiteral("Toggle autoclicking if already running."));
    parser.addOption(toggleOption);
    QCommandLineOption toggleIdOption(QStringLiteral("toggle-id"), QStringLiteral("Toggle autoclicking tabs that use the given hotkey."), QStringLiteral("hotkey"));
    parser.addOption(toggleIdOption);
    parser.process(app);

    const QString serverName = QStringLiteral("omniclicker-single-instance");
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(500)) {
        if (parser.isSet(toggleIdOption)) {
            socket.write(QStringLiteral("TOGGLE_ID:%1").arg(QUrl::fromPercentEncoding(parser.value(toggleIdOption).toUtf8())).toUtf8());
        } else if (parser.isSet(toggleOption)) {
            socket.write("TOGGLE");
        } else {
            socket.write("WAKEUP");
        }
        socket.flush();
        socket.waitForBytesWritten(500);
        return 0; // Exit since another instance is running
    }

    QLocalServer server;
    QLocalServer::removeServer(serverName);
    server.listen(serverName);

    MainWindow window;

    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket* client = server.nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead, [&window, client]() {
            QByteArray data = client->readAll();
            if (data == "WAKEUP") {
                window.showNormal();
                window.raise();
                window.activateWindow();
            } else if (data == "TOGGLE") {
                window.triggerHotkeyToggle();
            } else if (data.startsWith("TOGGLE_ID:")) {
                window.triggerHotkeyToggle(QString::fromUtf8(data.mid(10)));
            }
        });
        QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
    });

    window.show();

    if (parser.isSet(minimizedOption) || window.shouldStartMinimized()) {
        QTimer::singleShot(0, &window, [&window]() {
            window.setWindowState(window.windowState() | Qt::WindowMinimized);
        });
    }

    if (parser.isSet(toggleIdOption)) {
        window.triggerHotkeyToggle(QUrl::fromPercentEncoding(parser.value(toggleIdOption).toUtf8()));
    } else if (parser.isSet(toggleOption)) {
        window.triggerHotkeyToggle();
    }

    return QApplication::exec();
}
