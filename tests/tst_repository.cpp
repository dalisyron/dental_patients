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

    Patient mk(const QString& familyName, const QString& givenName, const QString& fileNo,
               const QString& phone = {}, const QString& notes = {}) {
        Patient p;
        p.familyName = familyName;
        p.givenName = givenName;
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
        QSqlQuery q(Database::instance().sql());
        QVERIFY(q.exec(QStringLiteral("DELETE FROM patients_trash;")));
        QVERIFY(q.exec(QStringLiteral("DELETE FROM patients;")));
    }

    void insertAndFind() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        auto id = insert(repo, mk(QStringLiteral("محمدی"), QStringLiteral("علی"), QStringLiteral("1234"),
                                  QStringLiteral("09120000001"), QStringLiteral("test")), &err);
        QVERIFY2(id.has_value(), qPrintable(err));
        QCOMPARE(repo.count(), 1);

        auto fetched = repo.findById(*id);
        QVERIFY(fetched.has_value());
        QCOMPARE(fetched->familyName, QStringLiteral("محمدی"));
        QCOMPARE(fetched->givenName, QStringLiteral("علی"));
        QCOMPARE(fetched->displayName(), QStringLiteral("علی محمدی"));
        QCOMPARE(fetched->fileNumber, QStringLiteral("1234"));
    }

    void allowsLegacyDuplicateFileNumberRows() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("الف"), QStringLiteral("الف"), QStringLiteral("100")), &err).has_value(), qPrintable(err));
        auto dupe = insert(repo, mk(QStringLiteral("ب"), QStringLiteral("ب"), QStringLiteral("100")), &err);
        QVERIFY2(dupe.has_value(), qPrintable(err));
        QCOMPARE(repo.count(), 2);
    }

    void update() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        auto id = insert(repo, mk(QStringLiteral("aaa"), QStringLiteral("reza"), QStringLiteral("9001")), &err);
        QVERIFY2(id.has_value(), qPrintable(err));
        Patient p = *repo.findById(*id);
        p.familyName = QStringLiteral("bbb");
        p.givenName = QStringLiteral("ali");
        p.notes = QStringLiteral("hello");
        QVERIFY(repo.update(p));
        auto re = repo.findById(*id);
        QCOMPARE(re->familyName, QStringLiteral("bbb"));
        QCOMPARE(re->givenName, QStringLiteral("ali"));
        QCOMPARE(re->displayName(), QStringLiteral("ali bbb"));
        QCOMPARE(re->notes, QStringLiteral("hello"));
    }

    void softDeleteAndRestore() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        auto id = insert(repo, mk(QStringLiteral("حذف"), QStringLiteral("شونده"), QStringLiteral("777")), &err);
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
        QString err;
        QVERIFY2(insert(repo, mk(QString::fromUtf8("اكبري"), QString::fromUtf8("علي"), QStringLiteral("321")), &err).has_value(), qPrintable(err));

        auto resA = repo.search(QString::fromUtf8("علي"));
        auto resB = repo.search(QString::fromUtf8("علی"));
        QCOMPARE(resA.size(), 1);
        QCOMPARE(resB.size(), 1);
    }

    void searchByFileNumber() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("X"), QStringLiteral("A"), QStringLiteral("5555")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("Y"), QStringLiteral("B"), QStringLiteral("5556")), &err).has_value(), qPrintable(err));

        auto p = repo.search(QString::fromUtf8("۵۵۵۵"));
        QCOMPARE(p.size(), 1);
        QCOMPARE(p.first().fileNumber, QStringLiteral("5555"));
    }

    void searchPrefix() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("ahmadi"), QStringLiteral("reza"), QStringLiteral("1")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("ahmadi"), QStringLiteral("ali"),  QStringLiteral("2")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("rezaei"), QStringLiteral("ali"),  QStringLiteral("3")), &err).has_value(), qPrintable(err));

        auto p = repo.search(QStringLiteral("ahm"));
        QCOMPARE(p.size(), 2);
    }

    void sortByFamilyNameTiesByGivenName() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("z"), QStringLiteral("c"), QStringLiteral("3")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("2")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("1")), &err).has_value(), qPrintable(err));

        auto rows = repo.search({}, 10, PatientRepository::SortField::FamilyName, true);
        QCOMPARE(rows.at(0).displayName(), QStringLiteral("a a"));
        QCOMPARE(rows.at(1).displayName(), QStringLiteral("b a"));
        QCOMPARE(rows.at(2).displayName(), QStringLiteral("c z"));
    }

    void sortByFileNumberUsesNumericOrder() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("x"), QStringLiteral("x"), QStringLiteral("10")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("y"), QStringLiteral("y"), QStringLiteral("2")), &err).has_value(), qPrintable(err));

        auto rows = repo.search({}, 10, PatientRepository::SortField::FileNumber, true);
        QCOMPARE(rows.at(0).fileNumber, QStringLiteral("2"));
        QCOMPARE(rows.at(1).fileNumber, QStringLiteral("10"));
    }

    void searchEmptyReturnsAll() {
        PatientRepository repo(Database::instance().sql());
        for (int i = 0; i < 5; ++i) {
            QString err;
            QVERIFY2(insert(repo, mk(QStringLiteral("p%1").arg(i), QStringLiteral("g"), QString::number(2000 + i)), &err).has_value(), qPrintable(err));
        }
        QCOMPARE(repo.search({}).size(), 5);
    }

    void insertManyHandlesDuplicates() {
        PatientRepository repo(Database::instance().sql());
        QVector<Patient> batch{
            mk(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("100")),
            mk(QStringLiteral("b"), QStringLiteral("b"), QStringLiteral("100")),
            mk(QStringLiteral("c"), QStringLiteral("c"), QStringLiteral("101")),
        };
        QString err;
        const int imported = repo.insertMany(batch, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(imported, 3);
    }

    void nextAvailableFileNumberStartsAt6000() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        const auto next = repo.nextAvailableFileNumber(6000, &err);
        QVERIFY2(next.has_value(), qPrintable(err));
        QCOMPARE(*next, QStringLiteral("6000"));
    }

    void nextAvailableFileNumberSkipsAssignedNumbers() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        QVERIFY2(insert(repo, mk(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("5999")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("b"), QStringLiteral("b"), QStringLiteral("6000")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("c"), QStringLiteral("c"), QStringLiteral("6001")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("d"), QStringLiteral("d"), QStringLiteral("6001")), &err).has_value(), qPrintable(err));
        QVERIFY2(insert(repo, mk(QStringLiteral("e"), QStringLiteral("e"), QStringLiteral("6003")), &err).has_value(), qPrintable(err));

        const auto next = repo.nextAvailableFileNumber(6000, &err);
        QVERIFY2(next.has_value(), qPrintable(err));
        QCOMPARE(*next, QStringLiteral("6002"));
    }

    void nextAvailableFileNumberSkipsTrashRows() {
        PatientRepository repo(Database::instance().sql());
        QString err;
        auto id = insert(repo, mk(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("6000")), &err);
        QVERIFY2(id.has_value(), qPrintable(err));
        QVERIFY(repo.softDelete(*id, &err));

        const auto next = repo.nextAvailableFileNumber(6000, &err);
        QVERIFY2(next.has_value(), qPrintable(err));
        QCOMPARE(*next, QStringLiteral("6001"));
    }

    void meta() {
        PatientRepository repo(Database::instance().sql());
        QVERIFY(repo.setMeta(QStringLiteral("k"), QStringLiteral("v1")));
        QCOMPARE(repo.meta(QStringLiteral("k")), QStringLiteral("v1"));
        QVERIFY(repo.setMeta(QStringLiteral("k"), QStringLiteral("v2")));
        QCOMPARE(repo.meta(QStringLiteral("k")), QStringLiteral("v2"));
    }

    void initializationState() {
        PatientRepository repo(Database::instance().sql());
        QVERIFY(!repo.isInitialized());
        QVERIFY(repo.markInitialized());
        QVERIFY(repo.isInitialized());
    }
};

QTEST_GUILESS_MAIN(TstRepository)
#include "tst_repository.moc"
