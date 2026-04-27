#include "db/Patient.h"
#include "ui/AnchoredPlaceholderPlainTextEdit.h"
#include "ui/PatientDialog.h"

#include <QLineEdit>
#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextOption>
#include <QWidget>

#include <array>

using namespace DentalPatients;

class TextFieldsTest : public QObject {
    Q_OBJECT

private slots:
    void patientDialogFieldsFollowDirectionRules();
    void patientDialogHonorsRequestedInitialFocus();
};

namespace {

QLineEdit* lineEditByPlaceholder(PatientDialog& dialog, const QString& placeholder) {
    const auto fields = dialog.findChildren<QLineEdit*>();
    for (auto* field : fields) {
        if (field->placeholderText() == placeholder) {
            return field;
        }
    }
    return nullptr;
}

void verifyPersianLineEdit(QLineEdit* field) {
    QVERIFY(field);
    QCOMPARE(field->layoutDirection(), Qt::RightToLeft);
    QVERIFY(field->alignment().testFlag(Qt::AlignRight));
    QVERIFY(field->alignment().testFlag(Qt::AlignAbsolute));
    QVERIFY(field->alignment().testFlag(Qt::AlignVCenter));
    QCOMPARE(field->cursorMoveStyle(), Qt::LogicalMoveStyle);
}

QWidget* fieldWidget(PatientDialog& dialog, PatientDialog::Field field) {
    switch (field) {
        case PatientDialog::Field::FamilyName:
            if (auto* target = dialog.findChild<QLineEdit*>(QStringLiteral("familyNameField"))) return target;
            return lineEditByPlaceholder(dialog, QString::fromUtf8("Ù…Ø«Ø§Ù„: Ù…Ø­Ù…Ø¯ÛŒ"));
        case PatientDialog::Field::GivenName:
            if (auto* target = dialog.findChild<QLineEdit*>(QStringLiteral("givenNameField"))) return target;
            return lineEditByPlaceholder(dialog, QString::fromUtf8("Ù…Ø«Ø§Ù„: Ø¹Ù„ÛŒ"));
        case PatientDialog::Field::FileNumber:
            if (auto* target = dialog.findChild<QLineEdit*>(QStringLiteral("fileNumberField"))) return target;
            return lineEditByPlaceholder(dialog, QString::fromUtf8("Ù…Ø«Ø§Ù„: 1234"));
        case PatientDialog::Field::Phone:
            if (auto* target = dialog.findChild<QLineEdit*>(QStringLiteral("phoneField"))) return target;
            return lineEditByPlaceholder(dialog, QString::fromUtf8("Ø§Ø®ØªÛŒØ§Ø±ÛŒ"));
        case PatientDialog::Field::Notes:
            if (auto* target = dialog.findChild<AnchoredPlaceholderPlainTextEdit*>(QStringLiteral("notesField"))) return target;
            return dialog.findChild<AnchoredPlaceholderPlainTextEdit*>();
    }
    return nullptr;
}

} // namespace

void TextFieldsTest::patientDialogFieldsFollowDirectionRules() {
    PatientDialog dialog(PatientDialog::Mode::Add, nullptr, Patient{});

    verifyPersianLineEdit(lineEditByPlaceholder(dialog, QString::fromUtf8("مثال: محمدی")));
    verifyPersianLineEdit(lineEditByPlaceholder(dialog, QString::fromUtf8("مثال: علی")));
    verifyPersianLineEdit(lineEditByPlaceholder(dialog, QString::fromUtf8("اختیاری")));

    auto* fileNumber = lineEditByPlaceholder(dialog, QString::fromUtf8("مثال: 1234"));
    QVERIFY(fileNumber);
    QCOMPARE(fileNumber->layoutDirection(), Qt::LeftToRight);
    QVERIFY(fileNumber->alignment().testFlag(Qt::AlignLeft));
    QVERIFY(fileNumber->alignment().testFlag(Qt::AlignAbsolute));
    QVERIFY(fileNumber->alignment().testFlag(Qt::AlignVCenter));
    QCOMPARE(fileNumber->cursorMoveStyle(), Qt::LogicalMoveStyle);

    auto* notes = dialog.findChild<AnchoredPlaceholderPlainTextEdit*>();
    QVERIFY(notes);
    QCOMPARE(notes->anchoredPlaceholderText(), QString::fromUtf8("اختیاری"));
    QCOMPARE(notes->placeholderText(), QString());
    QCOMPARE(notes->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(notes->document()->defaultTextOption().textDirection(), Qt::RightToLeft);
    QVERIFY(notes->document()->defaultTextOption().alignment().testFlag(Qt::AlignRight));
    QVERIFY(notes->document()->defaultTextOption().alignment().testFlag(Qt::AlignAbsolute));
    QCOMPARE(notes->document()->defaultCursorMoveStyle(), Qt::LogicalMoveStyle);
    QVERIFY(notes->tabChangesFocus());
    QCOMPARE(notes->textCursor().position(), 0);

    const QTextBlock firstBlock = notes->document()->firstBlock();
    QCOMPARE(firstBlock.blockFormat().layoutDirection(), Qt::RightToLeft);
    QVERIFY(firstBlock.blockFormat().alignment().testFlag(Qt::AlignRight));
    QVERIFY(firstBlock.blockFormat().alignment().testFlag(Qt::AlignAbsolute));
}

void TextFieldsTest::patientDialogHonorsRequestedInitialFocus() {
    const std::array<PatientDialog::Field, 5> fields = {
        PatientDialog::Field::FamilyName,
        PatientDialog::Field::GivenName,
        PatientDialog::Field::FileNumber,
        PatientDialog::Field::Phone,
        PatientDialog::Field::Notes
    };

    for (const PatientDialog::Field field : fields) {
        PatientDialog dialog(PatientDialog::Mode::Add, nullptr, Patient{}, field);
        QWidget* expected = fieldWidget(dialog, field);
        QVERIFY(expected);
        QCOMPARE(dialog.focusWidget(), expected);
    }
}

QTEST_MAIN(TextFieldsTest)

#include "tst_text_fields.moc"
