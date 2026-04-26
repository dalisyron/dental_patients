#include "db/Database.h"

#include "core/PersianText.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QtGlobal>
#include <QUuid>

namespace DentalPatients {

namespace {
QString g_dataDirOverride;
} // namespace

Database& Database::instance() {
    static Database d;
    return d;
}

void Database::setDataDirOverride(const QString& dir) {
    g_dataDirOverride = dir;
}

QString Database::defaultDataDir() {
    if (!g_dataDirOverride.isEmpty()) return g_dataDirOverride;
    QString dir;
#ifdef Q_OS_WIN
    const QString roaming = qEnvironmentVariable("APPDATA");
    if (!roaming.isEmpty()) {
        dir = QDir(roaming).filePath(QStringLiteral("DentalPatients"));
    }
#endif
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QDir().mkpath(dir);
    return dir;
}

QString Database::defaultDbPath() {
    return QDir(defaultDataDir()).filePath(QStringLiteral("patients.db"));
}

QString Database::backupDir() {
    const QString d = QDir(defaultDataDir()).filePath(QStringLiteral("backups"));
    QDir().mkpath(d);
    return d;
}

bool Database::open(QString* error) {
    return open(defaultDbPath(), error);
}

bool Database::open(const QString& path, QString* error) {
    if (m_db.isOpen()) close();

    QDir().mkpath(QFileInfo(path).absolutePath());

    // Unique connection name so multiple Database instances (in tests) don't clash.
    m_connectionName = QStringLiteral("dp_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        if (error) *error = m_db.lastError().text();
        return false;
    }
    m_path = path;

    // Robustness pragmas. NORMAL is the documented WAL recommendation - durable
    // across app crashes (only OS crash can lose the last write), and ~10x faster
    // than FULL on slow disks.
    if (!execNoResult(QStringLiteral("PRAGMA journal_mode = WAL;"), error))      return false;
    if (!execNoResult(QStringLiteral("PRAGMA synchronous = NORMAL;"), error))    return false;
    if (!execNoResult(QStringLiteral("PRAGMA foreign_keys = ON;"), error))       return false;
    if (!execNoResult(QStringLiteral("PRAGMA temp_store = MEMORY;"), error))     return false;
    if (!execNoResult(QStringLiteral("PRAGMA cache_size = -8000;"), error))      return false; // ~8 MB
    if (!execNoResult(QStringLiteral("PRAGMA busy_timeout = 5000;"), error))     return false;

    if (!integrityCheck(error)) return false;
    if (!runMigrations(error))  return false;

    return true;
}

void Database::close() {
    const QString name = m_connectionName;
    m_db.close();
    m_db = QSqlDatabase();
    if (!name.isEmpty()) QSqlDatabase::removeDatabase(name);
    m_connectionName.clear();
    m_path.clear();
}

bool Database::execNoResult(const QString& sql, QString* error) {
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        if (error) *error = QStringLiteral("%1\n  on: %2").arg(q.lastError().text(), sql);
        return false;
    }
    return true;
}

bool Database::integrityCheck(QString* error) {
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("PRAGMA integrity_check;"))) {
        if (error) *error = q.lastError().text();
        return false;
    }
    if (q.next()) {
        const QString result = q.value(0).toString();
        if (result.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
            if (error) *error = QStringLiteral("integrity_check failed: %1").arg(result);
            return false;
        }
    }
    return true;
}

bool Database::runMigrations(QString* error) {
    if (!execNoResult(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version INTEGER NOT NULL PRIMARY KEY"
        ");"), error)) return false;

    QSqlQuery q(m_db);
    int current = 0;
    if (q.exec(QStringLiteral("SELECT MAX(version) FROM schema_version;")) && q.next()) {
        current = q.value(0).toInt();
    }

    auto bump = [&](int v) {
        QSqlQuery ins(m_db);
        ins.prepare(QStringLiteral("INSERT INTO schema_version(version) VALUES(?);"));
        ins.addBindValue(v);
        return ins.exec();
    };

    // ----- Migration 1: core schema -----------------------------------------
    if (current < 1) {
        if (!m_db.transaction()) {
            if (error) *error = m_db.lastError().text();
            return false;
        }

        const QStringList stmts = {
            QStringLiteral(
                "CREATE TABLE patients ("
                "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  file_number TEXT    NOT NULL COLLATE NOCASE,"
                "  full_name   TEXT    NOT NULL,"
                "  search_text TEXT    NOT NULL,"
                "  phone       TEXT    NOT NULL DEFAULT '',"
                "  notes       TEXT    NOT NULL DEFAULT '',"
                "  created_at  INTEGER NOT NULL,"
                "  updated_at  INTEGER NOT NULL"
                ");"),
            QStringLiteral("CREATE INDEX idx_patients_file_number ON patients(file_number);"),
            QStringLiteral("CREATE INDEX idx_patients_full_name ON patients(full_name);"),
            QStringLiteral("CREATE INDEX idx_patients_search    ON patients(search_text);"),

            QStringLiteral(
                "CREATE TABLE patients_trash ("
                "  id          INTEGER PRIMARY KEY,"
                "  file_number TEXT    NOT NULL,"
                "  full_name   TEXT    NOT NULL,"
                "  search_text TEXT    NOT NULL,"
                "  phone       TEXT    NOT NULL,"
                "  notes       TEXT    NOT NULL,"
                "  created_at  INTEGER NOT NULL,"
                "  updated_at  INTEGER NOT NULL,"
                "  deleted_at  INTEGER NOT NULL"
                ");"),
            QStringLiteral("CREATE INDEX idx_trash_deleted ON patients_trash(deleted_at);"),

            // FTS5 over the normalised search_text + file_number + phone + notes.
            QStringLiteral(
                "CREATE VIRTUAL TABLE patients_fts USING fts5("
                "  search_text, file_number, phone, notes,"
                "  content='patients', content_rowid='id',"
                "  tokenize='unicode61 remove_diacritics 2'"
                ");"),
            QStringLiteral(
                "CREATE TRIGGER patients_ai AFTER INSERT ON patients BEGIN "
                "  INSERT INTO patients_fts(rowid, search_text, file_number, phone, notes) "
                "  VALUES (new.id, new.search_text, new.file_number, new.phone, new.notes); "
                "END;"),
            QStringLiteral(
                "CREATE TRIGGER patients_ad AFTER DELETE ON patients BEGIN "
                "  INSERT INTO patients_fts(patients_fts, rowid, search_text, file_number, phone, notes) "
                "  VALUES ('delete', old.id, old.search_text, old.file_number, old.phone, old.notes); "
                "END;"),
            QStringLiteral(
                "CREATE TRIGGER patients_au AFTER UPDATE ON patients BEGIN "
                "  INSERT INTO patients_fts(patients_fts, rowid, search_text, file_number, phone, notes) "
                "  VALUES ('delete', old.id, old.search_text, old.file_number, old.phone, old.notes); "
                "  INSERT INTO patients_fts(rowid, search_text, file_number, phone, notes) "
                "  VALUES (new.id, new.search_text, new.file_number, new.phone, new.notes); "
                "END;"),

            QStringLiteral(
                "CREATE TABLE meta ("
                "  key   TEXT PRIMARY KEY,"
                "  value TEXT NOT NULL"
                ");"),
        };

        for (const auto& s : stmts) {
            if (!execNoResult(s, error)) {
                m_db.rollback();
                return false;
            }
        }
        if (!bump(1) || !m_db.commit()) {
            if (error) *error = m_db.lastError().text();
            m_db.rollback();
            return false;
        }
        current = 1;
    }

    // ----- Migration 2: split names into family/given columns ---------------
    if (current < 2) {
        if (!m_db.transaction()) {
            if (error) *error = m_db.lastError().text();
            return false;
        }

        const QStringList stmts = {
            QStringLiteral("ALTER TABLE patients ADD COLUMN family_name TEXT NOT NULL DEFAULT '';"),
            QStringLiteral("ALTER TABLE patients ADD COLUMN given_name TEXT NOT NULL DEFAULT '';"),
            QStringLiteral("ALTER TABLE patients_trash ADD COLUMN family_name TEXT NOT NULL DEFAULT '';"),
            QStringLiteral("ALTER TABLE patients_trash ADD COLUMN given_name TEXT NOT NULL DEFAULT '';"),
            QStringLiteral("CREATE INDEX idx_patients_family_given ON patients(family_name, given_name);"),
        };

        for (const auto& s : stmts) {
            if (!execNoResult(s, error)) {
                m_db.rollback();
                return false;
            }
        }

        auto migrateRows = [&](const QString& table, bool hasFtsTriggers) -> bool {
            QSqlQuery select(m_db);
            if (!select.exec(QStringLiteral(
                    "SELECT id, full_name, file_number, phone, notes FROM %1;").arg(table))) {
                if (error) *error = select.lastError().text();
                return false;
            }

            while (select.next()) {
                const qint64 id = select.value(0).toLongLong();
                const auto parts = PersianText::splitFamilyGiven(select.value(1).toString());
                const QString display = PersianText::displayName(parts.givenName, parts.familyName);
                const QString searchText = PersianText::normalizeForSearch(
                    parts.familyName + QLatin1Char(' ') +
                    parts.givenName + QLatin1Char(' ') +
                    display + QLatin1Char(' ') +
                    select.value(2).toString() + QLatin1Char(' ') +
                    select.value(3).toString() + QLatin1Char(' ') +
                    select.value(4).toString());

                QSqlQuery update(m_db);
                update.prepare(QStringLiteral(
                    "UPDATE %1 SET family_name = ?, given_name = ?, full_name = ?, search_text = ? WHERE id = ?;"
                ).arg(table));
                update.addBindValue(parts.familyName);
                update.addBindValue(parts.givenName);
                update.addBindValue(display);
                update.addBindValue(searchText);
                update.addBindValue(id);
                if (!update.exec()) {
                    if (error) *error = update.lastError().text();
                    return false;
                }
            }

            if (hasFtsTriggers) {
                QSqlQuery rebuild(m_db);
                if (!rebuild.exec(QStringLiteral("INSERT INTO patients_fts(patients_fts) VALUES('rebuild');"))) {
                    if (error) *error = rebuild.lastError().text();
                    return false;
                }
            }
            return true;
        };

        if (!migrateRows(QStringLiteral("patients"), true) ||
            !migrateRows(QStringLiteral("patients_trash"), false) ||
            !bump(2) || !m_db.commit()) {
            if (error && error->isEmpty()) *error = m_db.lastError().text();
            m_db.rollback();
            return false;
        }
        current = 2;
    }

    // Future migrations: append `if (current < 3) { ... bump(3); }` blocks here.

    return true;
}

QString Database::createBackup(QString* error) {
    if (!m_db.isOpen()) {
        if (error) *error = QStringLiteral("database is not open");
        return {};
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString dest = QDir(backupDir()).filePath(QStringLiteral("patients-%1.db").arg(stamp));

    // Online backup via the SQLite VACUUM INTO statement - safe with WAL,
    // creates a consistent single-file copy without locking writers.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("VACUUM INTO ?;"));
    q.addBindValue(dest);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return {};
    }
    return dest;
}

bool Database::restoreFromBackup(const QString& backupPath, QString* error) {
    if (!QFile::exists(backupPath)) {
        if (error) *error = QStringLiteral("backup not found: %1").arg(backupPath);
        return false;
    }

    const QString live = m_path.isEmpty() ? defaultDbPath() : m_path;
    close();

    // Move the current file aside before clobbering it.
    if (QFile::exists(live)) {
        const QString aside = live + QStringLiteral(".pre-restore-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (!QFile::rename(live, aside)) {
            if (error) *error = QStringLiteral("could not move existing db aside: %1").arg(live);
            return false;
        }
        QFile::remove(live + QStringLiteral("-wal"));
        QFile::remove(live + QStringLiteral("-shm"));
    }

    if (!QFile::copy(backupPath, live)) {
        if (error) *error = QStringLiteral("could not copy backup -> live db");
        return false;
    }
    return open(live, error);
}

QStringList Database::listBackups() const {
    QDir d(backupDir());
    auto names = d.entryList(QStringList{QStringLiteral("patients-*.db")}, QDir::Files, QDir::Name | QDir::Reversed);
    for (auto& n : names) n = d.filePath(n);
    return names;
}

void Database::rotateBackups(int keepLast) {
    auto all = listBackups();        // newest first
    for (int i = keepLast; i < all.size(); ++i) {
        QFile::remove(all.at(i));
    }
}

bool Database::backupTodayIfMissing(QString* error) {
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    for (const auto& b : listBackups()) {
        if (QFileInfo(b).fileName().contains(today)) {
            return true;
        }
    }
    QString backupErr;
    const QString made = createBackup(&backupErr);
    if (made.isEmpty()) {
        if (error) *error = backupErr;
        return false;
    }
    rotateBackups(30);
    return true;
}

} // namespace DentalPatients
