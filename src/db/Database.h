#pragma once

#include <QString>
#include <QStringList>
#include <QSqlDatabase>

namespace DentalPatients {

// Owns a single SQLite connection at %APPDATA%\DentalPatients\patients.db.
// Configures WAL, FTS5, runs migrations, performs integrity checks and
// daily backups with rotation.
class Database {
public:
    static Database& instance();

    // Lifecycle
    bool open(QString* error = nullptr);
    bool open(const QString& path, QString* error = nullptr);
    void close();
    bool isOpen() const { return m_db.isOpen(); }
    QSqlDatabase& sql() { return m_db; }
    QString path() const { return m_path; }

    // Backup
    QString createBackup(QString* error = nullptr);          // returns full path of new backup
    bool    restoreFromBackup(const QString& backupPath, QString* error = nullptr);
    QStringList listBackups() const;
    void    rotateBackups(int keepLast = 30);
    bool    backupTodayIfMissing(QString* error = nullptr);  // called on app start

    // Integrity
    bool integrityCheck(QString* error = nullptr);

    // Standard locations - public so the UI / installer can show them.
    static QString defaultDataDir();
    static QString defaultDbPath();
    static QString backupDir();

    // Test hook: override the data dir.
    static void setDataDirOverride(const QString& dir);

private:
    Database() = default;
    bool runMigrations(QString* error);
    bool execNoResult(const QString& sql, QString* error);

    QSqlDatabase m_db;
    QString m_path;
    QString m_connectionName;
};

} // namespace DentalPatients
