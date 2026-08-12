#include "ui/TrashDialog.h"

#include "core/AppLanguage.h"
#include "core/PersianText.h"
#include "db/PatientRepository.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace DentalPatients {

namespace {

void showMessage(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text) {
    QMessageBox box(parent);
    box.setIcon(icon);
    box.setWindowTitle(title);
    box.setText(text);
    auto* okButton = box.addButton(TrashDialog::tr("OK"), QMessageBox::AcceptRole);
    box.setDefaultButton(okButton);
    box.setEscapeButton(okButton);
    box.exec();
}

void showInformation(QWidget* parent, const QString& title, const QString& text) {
    showMessage(parent, QMessageBox::Information, title, text);
}

void showCritical(QWidget* parent, const QString& title, const QString& text) {
    showMessage(parent, QMessageBox::Critical, title, text);
}

} // namespace

TrashDialog::TrashDialog(PatientRepository* repo, QWidget* parent)
    : QDialog(parent), m_repo(repo) {
    setWindowTitle(tr("Recycle bin"));
    setModal(true);
    resize(720, 480);
    buildUi();
    refresh();
}

void TrashDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* hint = new QLabel(tr("Deleted patients are kept here and can be restored when needed."));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({tr("Family name"), tr("Given name"), tr("Case number"), tr("Phone"), tr("Notes")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    auto* header = m_table->horizontalHeader();
    header->setHighlightSections(false);
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::Stretch);
    root->addWidget(m_table, 1);

    auto* buttons = new QHBoxLayout;
    auto* restore = new QPushButton(tr("Restore"));
    restore->setObjectName(QStringLiteral("primaryButton"));
    auto* close = new QPushButton(tr("Close"));
    buttons->addStretch(1);
    buttons->addWidget(restore);
    buttons->addWidget(close);
    root->addLayout(buttons);

    connect(restore, &QPushButton::clicked, this, &TrashDialog::onRestore);
    connect(close,   &QPushButton::clicked, this, &QDialog::accept);
}

void TrashDialog::refresh() {
    m_data = m_repo->trash(2000);
    m_table->setRowCount(m_data.size());
    for (int i = 0; i < m_data.size(); ++i) {
        const Patient& p = m_data.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(p.familyName));
        m_table->setItem(i, 1, new QTableWidgetItem(p.givenName));
        m_table->setItem(i, 2, new QTableWidgetItem(AppLanguage::localizeDigits(p.fileNumber)));
        m_table->setItem(i, 3, new QTableWidgetItem(AppLanguage::localizeDigits(p.phone)));
        m_table->setItem(i, 4, new QTableWidgetItem(p.notes));
    }
}

void TrashDialog::onRestore() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_data.size()) {
        showInformation(this, tr("Restore"), tr("Please select a row."));
        return;
    }
    const Patient& p = m_data.at(row);
    QString err;
    if (!m_repo->restoreFromTrash(p.id, &err)) {
        showCritical(this, tr("Error"),
            tr("Restore failed:\n%1").arg(err));
        return;
    }
    emit restored();
    refresh();
}

} // namespace DentalPatients
