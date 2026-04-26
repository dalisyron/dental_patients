#include "db/Database.h"
#include "db/Patient.h"
#include "db/PatientRepository.h"

#include <QDir>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace DentalPatients;

class TstRepository : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    Patient mk(const QString& name, const QString& fileNo,
               const QString& phone = {}, const QString& notes = {}) {
        Patient p;
        p.fullName = name;
        p.fileNumber = fileNo;
        p.phone = phone;
        p.notes = notes;
        return p;
    }

    std::optional<qint64> insert(PatientRepository& repo, const Patient& p, QString* error = nullptr) {
        QString localError;
        auto id = repo.insert(p, &localError);
        if (error) *error = localError;
        return id;
    }

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        Database::setDataDirOverride(m_dir.path());
        QString err;
        QVERIFY2(Database::instance().open(&err), qPrintable(err));
    }

    void cleanupTestCase() {
        Database::instance().close();
        Database::setDataDirOverride({});
    }

    void init() {
        // Truncate state between tests.
        QSqlQuery q(Database::instance().sql());
        QVERIFY(q.exec(QStringLiteral("DELETE FROM patients_trash;")));
        QVERIFY(q.exec(QStringLiteral("DELETE FROM patients;")));
    }

    void insertAndFind() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        auto id = insert(repo, mk(QStringLiteral("علی محمدی"), QStringLiteral("1234"), QStringLiteral("09120000001"), QStringLiteral("test")), &err);
        QVERIFY2(id.has_value(), qPrintable(err));
        QCOMPARE(repo.count(), 1);

        auto fetched = repo.findById(*id);
        QVERIFY(fetched.has_value());
        QCOMPARE(fetched->fullName, QStringLiteral("علی محمدی"));
        QCOMPARE(fetched->fileNumber, QStringLiteral("1234"));
    }

    void allowsLegacyDuplicateFileNumberRows() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("الف"), QStringLiteral("100")), &err).has_value(), qPrintable(err));
        auto dupe = insert(repo, mk(QStringLiteral("ب"), QStringLiteral("100")), &err);
        QVERIFY2(dupe.has_value(), qPrintable(err));
        QCOMPARE(repo.count(), 2);
    }

    void update() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        auto id = insert(repo, mk(QStringLiteral("aaa"), QStringLiteral("9001")), &err);
        QVERIFY2(id.has_value(), qPrintable(err));
        Patient p = *repo.findById(*id);
        p.fullName = QStringLiteral("bbb");
        p.notes = QStringLiteral("hello");
        QVERIFY(repo.update(p));
        auto re = repo.findById(*id);
        QCOMPARE(re->fullName, QStringLiteral("bbb"));
        QCOMPARE(re->notes, QStringLiteral("hello"));
    }

    void softDeleteAndRestore() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        auto id = insert(repo, mk(QStringLiteral("به‌حذف‌شو"), QStringLiteral("777")), &err);
        QVERIFY2(id.has_value(), qPrintable(err));
        QCOMPARE(repo.count(), 1);

        QVERIFY(repo.softDelete(*id));
        QCOMPARE(repo.count(), 0);
        QCOMPARE(repo.trashCount(), 1);

        QVERIFY(repo.restoreFromTrash(*id));
        QCOMPARE(repo.count(), 1);
        QCOMPARE(repo.trashCount(), 0);
    }

    void searchHandlesPersianVariants() {
        PatientRepository repo(Database::instance().sql());
        // Stored with Arabic Yeh.
        QString err;
        QVERIFY2(insert(repo, mk(QString::fromUtf8("علي اكبري"), QStringLiteral("321")), &err).has_value(), qPrintable(err));

        // Searching with Persian Yeh should still find it.
        auto resA = repo.search(QString::fromUtf8("علي"));
        auto resB = repo.search(QString::fromUtf8("علی"));
        QCOMPARE(resA.size(), 1);
        QCOMPARE(resB.size(), 1);
    }

    void searchByFileNumber() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("X"), QStringLiteral("5555")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("Y"), QStringLiteral("5556")), &err).has_value(), qPrintable(err));

        // Persian-digit query should still find ASCII-stored numbers.
        auto p = repo.search(QString::fromUtf8("۵۵۵۵"));
        QCOMPARE(p.size(), 1);
        QCOMPARE(p.first().fileNumber, QStringLiteral("5555"));
    }

    void searchPrefix() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("ahmadi reza"), QStringLiteral("1")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("ahmadi ali"),  QStringLiteral("2")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("rezaei"),       QStringLiteral("3")), &err).has_value(), qPrintable(err));

        auto p = repo.search(QStringLiteral("ahm"));
        QCOMPARE(p.size(), 2);
    }

    void searchEmptyReturnsAll() {
        PatientRepository repo(Database::instance().sql());
        for (int i = 0; i < 5; ++i) {
            QString err;
            QVERIFY2(insert(repo, mk(QStringLiteral("p%1").arg(i), QString::number(2000 + i)), &err).has_value(), qPrintable(err));
        }
        QCOMPARE(repo.search({}).size(), 5);
    }

    void insertManyHandlesDuplicates() {
        PatientRepository repo(Database::instance().sql());
        QVector<Patient> batch{
            mk(QStringLiteral("a"), QStringLiteral("100")),
            mk(QStringLiteral("b"), QStringLiteral("100")),  // legacy duplicate preserved
            mk(QStringLiteral("c"), QStringLiteral("101")),
        };
        int dupes = 0; QString err;
        const int imported = repo.insertMany(batch, &dupes, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(imported, 3);
        QCOMPARE(dupes, 0);
    }

    void meta() {
        PatientRepository repo(Database::instance().sql());
        QVERIFY(repo.setMeta(QStringLiteral("k"), QStringLiteral("v1")));
        QCOMPARE(repo.meta(QStringLiteral("k")), QStringLiteral("v1"));
        QVERIFY(repo.setMeta(QStringLiteral("k"), QStringLiteral("v2")));
        QCOMPARE(repo.meta(QStringLiteral("k")), QStringLiteral("v2"));
    }
};

QTEST_GUILESS_MAIN(TstRepository)
#include "tst_repository.moc"
