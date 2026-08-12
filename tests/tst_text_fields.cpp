#include "core/AppLanguage.h"
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
    void englishFieldsFollowDirectionRules();
    void persianFieldsFollowDirectionRules();
    void patientDialogHonorsRequestedInitialFocus();
};

namespace {

QLineEdit* lineEdit(PatientDialog& dialog, const char* objectName) {
    return dialog.findChild<QLineEdit*>(QLatin1String(objectName));
}

AnchoredPlaceholderPlainTextEdit* notesEdit(PatientDialog& dialog) {
    return dialog.findChild<AnchoredPlaceholderPlainTextEdit*>(QStringLiteral("notesField"));
}

void verifyPersianLineEdit(QLineEdit* field) {
    QVERIFY(field);
    QCOMPARE(field->layoutDirection(), Qt::RightToLeft);
    QVERIFY(field->alignment().testFlag(Qt::AlignRight));
    QVERIFY(field->alignment().testFlag(Qt::AlignAbsolute));
    QVERIFY(field->alignment().testFlag(Qt::AlignVCenter));
    QCOMPARE(field->cursorMoveStyle(), Qt::LogicalMoveStyle);
}

void verifyLatinLineEdit(QLineEdit* field) {
    QVERIFY(field);
    QCOMPARE(field->layoutDirection(), Qt::LeftToRight);
    QVERIFY(field->alignment().testFlag(Qt::AlignLeft));
    QVERIFY(field->alignment().testFlag(Qt::AlignAbsolute));
    QVERIFY(field->alignment().testFlag(Qt::AlignVCenter));
    QCOMPARE(field->cursorMoveStyle(), Qt::LogicalMoveStyle);
}

QWidget* fieldWidget(PatientDialog& dialog, PatientDialog::Field field) {
    switch (field) {
        case PatientDialog::Field::FamilyName: return lineEdit(dialog, "familyNameField");
        case PatientDialog::Field::GivenName:  return lineEdit(dialog, "givenNameField");
        case PatientDialog::Field::FileNumber: return lineEdit(dialog, "fileNumberField");
        case PatientDialog::Field::Phone:      return lineEdit(dialog, "phoneField");
        case PatientDialog::Field::Notes:      return notesEdit(dialog);
    }
    return nullptr;
}

} // namespace

void TextFieldsTest::englishFieldsFollowDirectionRules() {
    AppLanguage::overrideForTesting(AppLanguage::Language::English);
    PatientDialog dialog(PatientDialog::Mode::Add, nullptr, Patient{});

    // Free-text fields keep the natural left-to-right entry in English mode.
    for (const char* name : {"familyNameField", "givenNameField", "phoneField"}) {
        auto* field = lineEdit(dialog, name);
        QVERIFY(field);
        QCOMPARE(field->layoutDirection(), Qt::LeftToRight);
        QCOMPARE(field->cursorMoveStyle(), Qt::LogicalMoveStyle);
    }

    verifyLatinLineEdit(lineEdit(dialog, "fileNumberField"));

    auto* notes = notesEdit(dialog);
    QVERIFY(notes);
    QCOMPARE(notes->anchoredPlaceholderText(), QStringLiteral("Optional"));
    QCOMPARE(notes->placeholderText(), QString());
    QCOMPARE(notes->layoutDirection(), Qt::LeftToRight);
    QCOMPARE(notes->document()->defaultCursorMoveStyle(), Qt::LogicalMoveStyle);
    QVERIFY(notes->tabChangesFocus());
    QCOMPARE(notes->textCursor().position(), 0);
}

void TextFieldsTest::persianFieldsFollowDirectionRules() {
    AppLanguage::overrideForTesting(AppLanguage::Language::Persian);
    PatientDialog dialog(PatientDialog::Mode::Add, nullptr, Patient{});

    verifyPersianLineEdit(lineEdit(dialog, "familyNameField"));
    verifyPersianLineEdit(lineEdit(dialog, "givenNameField"));
    verifyPersianLineEdit(lineEdit(dialog, "phoneField"));

    // Case numbers stay Latin left-to-right in both languages.
    verifyLatinLineEdit(lineEdit(dialog, "fileNumberField"));

    auto* notes = notesEdit(dialog);
    QVERIFY(notes);
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

    AppLanguage::overrideForTesting(AppLanguage::Language::English);
}

void TextFieldsTest::patientDialogHonorsRequestedInitialFocus() {
    AppLanguage::overrideForTesting(AppLanguage::Language::English);
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
