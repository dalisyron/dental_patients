#pragma once

#include <QLocalServer>
#include <QObject>
#include <QStringList>

namespace DentalPatients {

// Cooperative single-instance lock backed by a Qt QLocalServer (named pipe on
// Windows). The first launch becomes the primary; subsequent launches forward
// their argv to the primary and exit, so double-clicking the desktop shortcut
// or a .dpbackup file always activates the existing window.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(QObject* parent = nullptr);

    // True if we became the primary; false if another instance was already
    // running and we forwarded `args` to it.
    bool tryAcquire(const QStringList& args);

signals:
    void secondInstanceLaunched(const QStringList& args);

private slots:
    void onNewConnection();

private:
    QLocalServer m_server;
};

} // namespace DentalPatients
