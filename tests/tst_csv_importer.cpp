#include "db/CsvImporter.h"
#include "db/Database.h"
#include "db/PatientRepository.h"

#include <QCoreApplication>
#include <QFile>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace DentalPatients;

class TstCsvImporter : public QObject {
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
        QVERIFY(q.exec(QStringLiteral("DELETE FROM patients;")));
    }

    void importsBomPrefixedCsv() {
        const QString path = m_dir.filePath(QStringLiteral("seed.csv"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts.setGenerateByteOrderMark(true);
            ts << "Patient Name,Case Number\n";
            ts << "محمدی علی,1234\n";
            ts << "حسینی زهرا,5678\n";
        }
        PatientRepository repo(Database::instance().sql());
        auto r = CsvImporter::importFromFile(path, repo);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.imported, 2);
        QCOMPARE(repo.count(), 2);

        auto p = repo.findByFileNumber(QStringLiteral("1234"));
        QVERIFY(p.has_value());
        QCOMPARE(p->familyName, QStringLiteral("محمدی"));
        QCOMPARE(p->givenName, QStringLiteral("علی"));
        QCOMPARE(p->displayName(), QStringLiteral("علی محمدی"));
    }

    void quotedFieldsAndCommas() {
        const QString path = m_dir.filePath(QStringLiteral("quoted.csv"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts << "Patient Name,Case Number\n";
            ts << "\"محمدی, علی\",1234\n";
            ts << "\"حسینی \"\"محمد\"\"\",5678\n";
        }
        PatientRepository repo(Database::instance().sql());
        auto r = CsvImporter::importFromFile(path, repo);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.imported, 2);
        auto p = repo.findByFileNumber(QStringLiteral("1234"));
        QVERIFY(p.has_value());
        QCOMPARE(p->familyName, QStringLiteral("محمدی,"));
        QCOMPARE(p->givenName, QStringLiteral("علی"));
    }

    void importsRealSeedCsvIfPresent() {
        const QString seed = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/patient_list_merged_sorted.csv");
        if (!QFile::exists(seed)) QSKIP("seed CSV not found beside test binary");
        PatientRepository repo(Database::instance().sql());
        auto r = CsvImporter::importFromFile(seed, repo);
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(r.imported > 0);
        QVERIFY(repo.count() == r.imported);
        QCOMPARE(r.imported, 3365);

        auto first = repo.findByFileNumber(QStringLiteral("2910"));
        QVERIFY(first.has_value());
        QCOMPARE(first->familyName, QStringLiteral("اباذری"));
        QCOMPARE(first->givenName, QStringLiteral("صدیقه"));
    }
};

QTEST_GUILESS_MAIN(TstCsvImporter)
#include "tst_csv_importer.moc"
