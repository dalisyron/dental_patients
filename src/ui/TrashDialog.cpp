#include "ui/TrashDialog.h"

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

TrashDialog::TrashDialog(PatientRepository* repo, QWidget* parent)
    : QDialog(parent), m_repo(repo) {
    setWindowTitle(tr("سطل بازیافت"));
    setModal(true);
    setLayoutDirection(Qt::RightToLeft);
    resize(720, 480);
    buildUi();
    refresh();
}

void TrashDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* hint = new QLabel(tr("بیماران حذف‌شده در اینجا نگه‌داری می‌شوند تا در صورت نیاز بازگردانده شوند."));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({tr("نام خانوادگی"), tr("نام"), tr("شماره پرونده"), tr("تلفن"), tr("یادداشت")});
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
    auto* restore = new QPushButton(tr("بازگردانی"));
    restore->setObjectName(QStringLiteral("primaryButton"));
    auto* close = new QPushButton(tr("بستن"));
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
        m_table->setItem(i, 2, new QTableWidgetItem(PersianText::toPersianDigits(p.fileNumber)));
        m_table->setItem(i, 3, new QTableWidgetItem(PersianText::toPersianDigits(p.phone)));
        m_table->setItem(i, 4, new QTableWidgetItem(p.notes));
    }
}

void TrashDialog::onRestore() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_data.size()) {
        QMessageBox::information(this, tr("بازگردانی"), tr("لطفاً یک ردیف را انتخاب کنید."));
        return;
    }
    const Patient& p = m_data.at(row);
    QString err;
    if (!m_repo->restoreFromTrash(p.id, &err)) {
        QMessageBox::critical(this, tr("خطا"),
            tr("بازگردانی با خطا مواجه شد:\n%1").arg(err));
        return;
    }
    emit restored();
    refresh();
}

} // namespace DentalPatients
