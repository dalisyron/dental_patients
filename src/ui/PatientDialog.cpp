#include "ui/PatientDialog.h"

#include "core/PersianText.h"
#include "db/PatientRepository.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QVBoxLayout>

#include <utility>

namespace DentalPatients {

namespace {

void configurePersianLineEdit(QLineEdit* field) {
    field->setLayoutDirection(Qt::RightToLeft);
    field->setAlignment(Qt::AlignRight | Qt::AlignAbsolute | Qt::AlignVCenter);
    field->setCursorMoveStyle(Qt::LogicalMoveStyle);
}

void configurePersianPlainTextEdit(QPlainTextEdit* field) {
    field->setLayoutDirection(Qt::RightToLeft);

    QTextOption option = field->document()->defaultTextOption();
    option.setTextDirection(Qt::RightToLeft);
    option.setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    field->document()->setDefaultTextOption(option);
    field->document()->setDefaultCursorMoveStyle(Qt::LogicalMoveStyle);

    QTextBlockFormat format;
    format.setLayoutDirection(Qt::RightToLeft);
    format.setAlignment(Qt::AlignRight | Qt::AlignAbsolute);

    QTextCursor cursor(field->document());
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(format);
}

} // namespace

PatientDialog::PatientDialog(Mode mode, PatientRepository* repo, Patient initial, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_repo(repo), m_current(std::move(initial)) {
    m_originalId = m_current.id;
    m_originalFileNumber = PersianText::toAsciiDigits(m_current.fileNumber.trimmed());
    setWindowTitle(mode == Mode::Add ? tr("افزودن بیمار جدید") : tr("ویرایش اطلاعات بیمار"));
    setModal(true);
    setLayoutDirection(Qt::RightToLeft);
    resize(560, 480);
    buildUi();
}

void PatientDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(10);

    m_familyName = new QLineEdit(m_current.familyName);
    m_familyName->setPlaceholderText(tr("مثال: محمدی"));
    configurePersianLineEdit(m_familyName);
    form->addRow(tr("نام خانوادگی *"), m_familyName);

    m_givenName = new QLineEdit(m_current.givenName);
    m_givenName->setPlaceholderText(tr("مثال: علی"));
    configurePersianLineEdit(m_givenName);
    form->addRow(tr("نام *"), m_givenName);

    m_fileNumber = new QLineEdit(m_current.fileNumber);
    m_fileNumber->setPlaceholderText(tr("مثال: 1234"));
    form->addRow(tr("شماره پرونده *"), m_fileNumber);

    m_phone = new QLineEdit(m_current.phone);
    m_phone->setPlaceholderText(tr("اختیاری"));
    configurePersianLineEdit(m_phone);
    form->addRow(tr("تلفن"), m_phone);

    m_notes = new QPlainTextEdit(m_current.notes);
    m_notes->setPlaceholderText(tr("اختیاری"));
    configurePersianPlainTextEdit(m_notes);
    m_notes->setMinimumHeight(160);
    form->addRow(tr("یادداشت"), m_notes);

    root->addLayout(form);

    m_errorLabel = new QLabel;
    m_errorLabel->setObjectName(QStringLiteral("errorLabel"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);
    root->addWidget(m_errorLabel);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);

    m_saveBtn = new QPushButton(tr("ذخیره"));
    m_saveBtn->setObjectName(QStringLiteral("primaryButton"));
    m_saveBtn->setDefault(true);

    auto* cancel = new QPushButton(tr("انصراف"));

    buttons->addStretch(1);
    buttons->addWidget(m_saveBtn);
    buttons->addWidget(cancel);
    root->addLayout(buttons);

    connect(m_saveBtn, &QPushButton::clicked, this, &PatientDialog::onSave);
    connect(cancel,    &QPushButton::clicked, this, &QDialog::reject);
    connect(m_familyName, &QLineEdit::textChanged, this, &PatientDialog::onValidate);
    connect(m_givenName,  &QLineEdit::textChanged, this, &PatientDialog::onValidate);
    connect(m_fileNumber, &QLineEdit::textChanged, this, &PatientDialog::onValidate);

    onValidate();
}

bool PatientDialog::validate(QString* error) const {
    if (m_familyName->text().trimmed().isEmpty()) {
        if (error) *error = tr("نام خانوادگی الزامی است.");
        return false;
    }
    if (m_givenName->text().trimmed().isEmpty()) {
        if (error) *error = tr("نام الزامی است.");
        return false;
    }
    if (m_fileNumber->text().trimmed().isEmpty()) {
        if (error) *error = tr("شماره پرونده الزامی است.");
        return false;
    }
    return true;
}

void PatientDialog::onValidate() {
    QString err;
    const bool ok = validate(&err);
    m_saveBtn->setEnabled(ok);
    m_errorLabel->setText(err);
    m_errorLabel->setVisible(!ok);
}

void PatientDialog::onSave() {
    QString err;
    if (!validate(&err)) {
        QMessageBox::warning(this, tr("خطا"), err);
        return;
    }

    Patient p = m_current;
    p.familyName = m_familyName->text().trimmed();
    p.givenName  = m_givenName->text().trimmed();
    p.fileNumber = PersianText::toAsciiDigits(m_fileNumber->text().trimmed());
    p.phone      = PersianText::toAsciiDigits(m_phone->text().trimmed());
    p.notes      = m_notes->toPlainText().trimmed();

    // The legacy CSV contains a small number of duplicate file numbers. Preserve
    // them, but make new duplicates an explicit user decision.
    if (m_mode == Mode::Add || p.fileNumber != m_originalFileNumber) {
        auto existing = m_repo->findByFileNumber(p.fileNumber);
        if (existing && existing->id != m_originalId) {
            const auto reply = QMessageBox::question(this, tr("شماره پرونده تکراری"),
                tr("این شماره پرونده قبلاً برای بیمار «%1» ثبت شده است.\n"
                   "آیا می‌خواهید با همین شماره پرونده ذخیره شود؟").arg(existing->displayName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes) return;
        }
    }

    QString opErr;
    bool ok = false;
    if (m_mode == Mode::Add) {
        auto id = m_repo->insert(p, &opErr);
        if (id) { p.id = *id; ok = true; }
    } else {
        ok = m_repo->update(p, &opErr);
    }
    if (!ok) {
        QMessageBox::critical(this, tr("خطای پایگاه داده"),
            tr("ذخیره با خطا مواجه شد:\n%1").arg(opErr));
        return;
    }
    m_current = p;
    accept();
}

} // namespace DentalPatients
