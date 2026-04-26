#include "core/PersianText.h"

#include <QTest>

using namespace DentalPatients;

class TstPersianText : public QObject {
    Q_OBJECT
private slots:
    void normalizesArabicYehAndKaf() {
        // Input has Arabic Yeh (0x064A) and Kaf (0x0643).
        const QString in = QString::fromUtf8("علي كريمي");
        const QString out = PersianText::normalize(in);
        QVERIFY(out.contains(QChar(0x06CC)));   // Persian Yeh present
        QVERIFY(out.contains(QChar(0x06A9)));   // Persian Keh present
        QVERIFY(!out.contains(QChar(0x064A)));  // Arabic Yeh gone
        QVERIFY(!out.contains(QChar(0x0643)));  // Arabic Kaf gone
    }

    void stripsHarakatAndZwnj() {
        const QString in = QString::fromUtf8("مَن‌می‌روم");
        const QString out = PersianText::normalize(in);
        QVERIFY(!out.contains(QChar(0x064E))); // fatha
        QVERIFY(!out.contains(QChar(0x200C))); // ZWNJ
    }

    void convertsAsciiAndArabicIndicDigits() {
        QCOMPARE(PersianText::normalize(QStringLiteral("123")),
                 QString::fromUtf8("۱۲۳"));
        const QString arabicIndic = QString::fromUtf8("١٢٣");
        QCOMPARE(PersianText::normalize(arabicIndic),
                 QString::fromUtf8("۱۲۳"));
    }

    void searchVariantsCompareEqual() {
        // Both spellings of "علی" (Arabic vs Persian Yeh) must normalise to the same string.
        const QString a = QString::fromUtf8("علي");      // Arabic Yeh
        const QString b = QString::fromUtf8("علی");      // Persian Yeh
        QCOMPARE(PersianText::normalize(a), PersianText::normalize(b));
    }

    void asciiAndPersianDigitsRoundtrip() {
        const QString p = PersianText::toPersianDigits(QStringLiteral("0123456789"));
        QCOMPARE(p, QString::fromUtf8("۰۱۲۳۴۵۶۷۸۹"));
        QCOMPARE(PersianText::toAsciiDigits(p), QStringLiteral("0123456789"));
    }

    void collapsesWhitespace() {
        const QString in = QStringLiteral("  ali   ahmadi  ");
        QCOMPARE(PersianText::normalize(in), QStringLiteral("ali ahmadi"));
    }

    void splitsClinicCsvNameOrder() {
        const auto p = PersianText::splitFamilyGiven(QStringLiteral("ابراهیمی منش سید صادق"));
        QCOMPARE(p.familyName, QStringLiteral("ابراهیمی منش سید"));
        QCOMPARE(p.givenName, QStringLiteral("صادق"));
        QCOMPARE(PersianText::displayName(p.givenName, p.familyName),
                 QStringLiteral("صادق ابراهیمی منش سید"));
    }
};

QTEST_GUILESS_MAIN(TstPersianText)
#include "tst_persian_text.moc"
