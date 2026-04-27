#include "core/SingleInstance.h"

#include <QByteArray>
#include <QDataStream>
#include <QLocalSocket>

#include <memory>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace DentalPatients {

namespace {
constexpr auto kServerName        = "DentalPatients.SingleInstance.v1";
constexpr int  kHandshakeTimeoutMs = 1500;
} // namespace

SingleInstance::SingleInstance(QObject* parent) : QObject(parent) {
    connect(&m_server, &QLocalServer::newConnection,
            this, &SingleInstance::onNewConnection);
}

bool SingleInstance::tryAcquire(const QStringList& args) {
    {
        QLocalSocket probe;
        probe.connectToServer(QString::fromLatin1(kServerName));
        if (probe.waitForConnected(kHandshakeTimeoutMs)) {
            // An existing instance owns the lock. Allow it to take foreground
            // when it activates its window in response to our message - Windows
            // otherwise blocks SetForegroundWindow from a background process.
#ifdef Q_OS_WIN
            AllowSetForegroundWindow(ASFW_ANY);
#endif
            QByteArray payload;
            QDataStream out(&payload, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << args;
            probe.write(payload);
            probe.waitForBytesWritten(kHandshakeTimeoutMs);
            probe.disconnectFromServer();
            if (probe.state() != QLocalSocket::UnconnectedState) {
                probe.waitForDisconnected(kHandshakeTimeoutMs);
            }
            return false;
        }
    }

    // No primary running. Clear any stale socket file (no-op on Windows pipes
    // since they auto-cleanup) and start listening.
    QLocalServer::removeServer(QString::fromLatin1(kServerName));
    m_server.listen(QString::fromLatin1(kServerName));
    return true;
}

void SingleInstance::onNewConnection() {
    auto* sock = m_server.nextPendingConnection();
    if (!sock) return;

    auto buffer = std::make_shared<QByteArray>();
    connect(sock, &QLocalSocket::readyRead, this, [sock, buffer] {
        buffer->append(sock->readAll());
    });
    connect(sock, &QLocalSocket::disconnected, this, [this, sock, buffer] {
        buffer->append(sock->readAll());
        QDataStream in(buffer.get(), QIODevice::ReadOnly);
        in.setVersion(QDataStream::Qt_6_0);
        QStringList args;
        in >> args;
        if (in.status() == QDataStream::Ok) {
            emit secondInstanceLaunched(args);
        }
        sock->deleteLater();
    });
}

} // namespace DentalPatients
