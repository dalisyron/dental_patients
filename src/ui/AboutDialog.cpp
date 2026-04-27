#include "ui/AboutDialog.h"

#include "Version.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace DentalPatients {

namespace {

QLabel* shortcutKeyLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName(QStringLiteral("shortcutKeyLabel"));
    label->setLayoutDirection(Qt::LeftToRight);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumWidth(88);
    return label;
}

void addShortcutRow(QGridLayout* layout, int row, const QString& action, const QString& shortcut) {
    auto* actionLabel = new QLabel(action);
    actionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(actionLabel, row, 0);
    layout->addWidget(shortcutKeyLabel(shortcut), row, 1);
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("درباره برنامه"));
    setModal(true);
    setLayoutDirection(Qt::RightToLeft);
    resize(480, 430);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);

    auto* title = new QLabel(QString::fromUtf8(Version::kAppNameFa));
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    title->setFont(f);
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* version = new QLabel(tr("نسخه %1").arg(QString::fromUtf8(Version::kString)));
    version->setAlignment(Qt::AlignCenter);
    root->addWidget(version);

    auto* creator = new QLabel(tr("ساخته شده توسط: مبین داریوش"));
    creator->setAlignment(Qt::AlignCenter);
    root->addWidget(creator);

    auto* desc = new QLabel(tr(
        "نرم‌افزار مدیریت پرونده بیماران دندانپزشکی.\n"
        "تمام اطلاعات روی همین رایانه نگه‌داری می‌شود و نیازی به اینترنت ندارد."));
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    root->addWidget(desc);

    auto* shortcutsFrame = new QFrame;
    shortcutsFrame->setObjectName(QStringLiteral("shortcutsFrame"));
    auto* shortcutsLayout = new QVBoxLayout(shortcutsFrame);
    shortcutsLayout->setContentsMargins(14, 12, 14, 12);
    shortcutsLayout->setSpacing(10);

    auto* shortcutsTitle = new QLabel(tr("میانبرهای صفحه‌کلید"));
    QFont titleFont = shortcutsTitle->font();
    titleFont.setBold(true);
    shortcutsTitle->setFont(titleFont);
    shortcutsTitle->setAlignment(Qt::AlignRight);
    shortcutsLayout->addWidget(shortcutsTitle);

    auto* shortcutsGrid = new QGridLayout;
    shortcutsGrid->setContentsMargins(0, 0, 0, 0);
    shortcutsGrid->setHorizontalSpacing(18);
    shortcutsGrid->setVerticalSpacing(8);
    shortcutsGrid->setColumnStretch(0, 1);
    shortcutsGrid->setColumnStretch(1, 0);

    addShortcutRow(shortcutsGrid, 0, tr("افزودن بیمار جدید"), QStringLiteral("Ctrl+N"));
    addShortcutRow(shortcutsGrid, 1, tr("ویرایش بیمار انتخاب‌شده"), QStringLiteral("F2 / Enter"));
    addShortcutRow(shortcutsGrid, 2, tr("حذف بیمار انتخاب‌شده"), QStringLiteral("Delete"));
    addShortcutRow(shortcutsGrid, 3, tr("رفتن به جستجو"), QStringLiteral("Ctrl+F"));
    shortcutsLayout->addLayout(shortcutsGrid);
    root->addWidget(shortcutsFrame);

    root->addStretch(1);

    auto* btns = new QHBoxLayout;
    btns->addStretch(1);
    auto* ok = new QPushButton(tr("بستن"));
    ok->setDefault(true);
    btns->addWidget(ok);
    btns->addStretch(1);
    root->addLayout(btns);

    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace DentalPatients
