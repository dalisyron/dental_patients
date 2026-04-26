#include "db/Database.h"
#include "db/PatientRepository.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace DentalPatients;

namespace {

bool exec(QSqlDatabase& db, const QString& sql, QString* error) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        if (error) *error = q.lastError().text() + QStringLiteral(" on: ") + sql;
        return false;
    }
    return true;
}

bool createLegacyV1Database(const QString& path, QString* error) {
    const QString connection = QStringLiteral("legacy_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(path);
    if (!db.open()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    const QStringList stmts = {
        QStringLiteral(
            "CREATE TABLE schema_version ("
            "  version INTEGER NOT NULL PRIMARY KEY"
            ");"),
        QStringLiteral("INSERT INTO schema_version(version) VALUES(1);"),
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
        QStringLiteral("CREATE INDEX idx_patients_search ON patients(search_text);"),
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
        if (!exec(db, s, error)) {
            db.close();
            QSqlDatabase::removeDatabase(connection);
            return false;
        }
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO patients(file_number, full_name, search_text, phone, notes, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, 1, 1);"));
    for (int i = 0; i < 100; ++i) {
        const QString fileNumber = QString::number(1000 + i);
        const QString fullName = (i == 7)
            ? QString::fromUtf8("صدیقه")
            : (i % 2 == 0)
            ? QString::fromUtf8("اباذری صدیقه")
            : QString::fromUtf8("محمدی علی رضا");
        insert.bindValue(0, fileNumber);
        insert.bindValue(1, fullName);
        insert.bindValue(2, fullName + QLatin1Char(' ') + fileNumber);
        insert.bindValue(3, QStringLiteral("0912000%1").arg(i, 3, 10, QLatin1Char('0')));
        insert.bindValue(4, QStringLiteral("note %1").arg(i));
        if (!insert.exec()) {
            if (error) *error = insert.lastError().text();
            db.close();
            QSqlDatabase::removeDatabase(connection);
            return false;
        }
    }
    insert.finish();
    insert = QSqlQuery();

    db.close();
    QSqlDatabase::removeDatabase(connection);
    return true;
}

} // namespace

class TstMigrations : public QObject {
    Q_OBJECT
private slots:
    void legacyV1DatabaseSplitsNamesOnOpen() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = QDir(dir.path()).filePath(QStringLiteral("patients.db"));

        QString err;
        QVERIFY2(createLegacyV1Database(dbPath, &err), qPrintable(err));
        QVERIFY2(Database::instance().open(dbPath, &err), qPrintable(err));

        PatientRepository repo(Database::instance().sql());
        QCOMPARE(repo.count(), 100);

        const auto first = repo.findByFileNumber(QStringLiteral("1000"));
        QVERIFY(first.has_value());
        QCOMPARE(first->familyName, QString::fromUtf8("اباذری"));
        QCOMPARE(first->givenName, QString::fromUtf8("صدیقه"));
        QCOMPARE(first->displayName(), QString::fromUtf8("صدیقه اباذری"));

        const auto second = repo.findByFileNumber(QStringLiteral("1001"));
        QVERIFY(second.has_value());
        QCOMPARE(second->familyName, QString::fromUtf8("محمدی علی"));
        QCOMPARE(second->givenName, QString::fromUtf8("رضا"));

        const auto singleToken = repo.findByFileNumber(QStringLiteral("1007"));
        QVERIFY(singleToken.has_value());
        QCOMPARE(singleToken->familyName, QStringLiteral(""));
        QCOMPARE(singleToken->givenName, QString::fromUtf8("صدیقه"));

        Database::instance().close();
    }

};

QTEST_GUILESS_MAIN(TstMigrations)
#include "tst_migrations.moc"
