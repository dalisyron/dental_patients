#include "db/Database.h"
#include "db/Patient.h"
#include "db/PatientRepository.h"

#include <QFile>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace DentalPatients;

class TstBackupRestore : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        Database::setDataDirOverride(m_dir.path());
        QVERIFY(Database::instance().open());
    }

    void cleanupTestCase() {
        Database::instance().close();
        Database::setDataDirOverride({});
    }

    void init() {
        QSqlQuery q(Database::instance().sql());
        QVERIFY(q.exec(QStringLiteral("DELETE FROM patients_trash;")));
        QVERIFY(q.exec(QStringLiteral("DELETE FROM patients;")));
    }

    void backupCreatedAndListed() {
        PatientRepository repo(Database::instance().sql());
        Patient p; p.familyName = QStringLiteral("a"); p.givenName = QStringLiteral("a"); p.fileNumber = QStringLiteral("1");
        QString err;
        QVERIFY2(repo.insert(p, &err).has_value(), qPrintable(err));

        const QString backup = Database::instance().createBackup(&err);
        QVERIFY2(!backup.isEmpty(), qPrintable(err));
        QVERIFY(QFile::exists(backup));

        const auto list = Database::instance().listBackups();
        QVERIFY(list.contains(backup));
    }

    void restoreReplacesData() {
        // Take a backup with one row, then add another row, then restore the
        // backup - second row should be gone.
        PatientRepository repo(Database::instance().sql());
        Patient p; p.familyName = QStringLiteral("a"); p.givenName = QStringLiteral("a"); p.fileNumber = QStringLiteral("1");
        QString err;
        QVERIFY2(repo.insert(p, &err).has_value(), qPrintable(err));
        const QString backup = Database::instance().createBackup();
        QVERIFY(!backup.isEmpty());

        Patient p2; p2.familyName = QStringLiteral("b"); p2.givenName = QStringLiteral("b"); p2.fileNumber = QStringLiteral("2");
        QVERIFY2(repo.insert(p2, &err).has_value(), qPrintable(err));
        QCOMPARE(repo.count(), 2);

        QVERIFY2(Database::instance().restoreFromBackup(backup, &err), qPrintable(err));

        PatientRepository repo2(Database::instance().sql());
        QCOMPARE(repo2.count(), 1);
        auto only = repo2.findByFileNumber(QStringLiteral("1"));
        QVERIFY(only.has_value());
    }

    void rotateKeepsOnlyN() {
        for (int i = 0; i < 5; ++i) {
            QTest::qSleep(1100);   // backups carry second-resolution timestamps
            QVERIFY(!Database::instance().createBackup().isEmpty());
        }
        Database::instance().rotateBackups(2);
        QCOMPARE(Database::instance().listBackups().size(), 2);
    }
};

QTEST_GUILESS_MAIN(TstBackupRestore)
#include "tst_backup_restore.moc"
