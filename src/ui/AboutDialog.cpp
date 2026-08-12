#include "ui/AboutDialog.h"

#include "Version.h"
#include "core/AppLanguage.h"

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
    label->setAlignment(Qt::AlignCenter | Qt::AlignAbsolute);
    label->setMinimumWidth(88);
    return label;
}

void addShortcutRow(QGridLayout* layout, int row, const QString& action, const QString& shortcut) {
    auto* actionLabel = new QLabel(action);
    if (AppLanguage::isPersian()) {
        actionLabel->setAlignment(Qt::AlignRight | Qt::AlignAbsolute | Qt::AlignVCenter);
    } else {
        actionLabel->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute | Qt::AlignVCenter);
    }
    layout->addWidget(actionLabel, row, 0);
    layout->addWidget(shortcutKeyLabel(shortcut), row, 1);
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("About"));
    setModal(true);
    resize(480, 430);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);

    auto* title = new QLabel(AppLanguage::appDisplayName());
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    title->setFont(f);
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* version = new QLabel(tr("Version %1").arg(
        AppLanguage::localizeDigits(QString::fromUtf8(Version::kString))));
    version->setAlignment(Qt::AlignCenter);
    root->addWidget(version);

    auto* creator = new QLabel(tr("Created by Mobin Dariush"));
    creator->setAlignment(Qt::AlignCenter);
    root->addWidget(creator);

    auto* desc = new QLabel(tr(
        "Offline dental patient records manager.\n"
        "All data stays on this computer; no internet is required."));
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    root->addWidget(desc);

    auto* shortcutsFrame = new QFrame;
    shortcutsFrame->setObjectName(QStringLiteral("shortcutsFrame"));
    auto* shortcutsLayout = new QVBoxLayout(shortcutsFrame);
    shortcutsLayout->setContentsMargins(14, 12, 14, 12);
    shortcutsLayout->setSpacing(10);

    auto* shortcutsTitle = new QLabel(tr("Keyboard shortcuts"));
    QFont titleFont = shortcutsTitle->font();
    titleFont.setBold(true);
    shortcutsTitle->setFont(titleFont);
    if (AppLanguage::isPersian()) {
        shortcutsTitle->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    } else {
        shortcutsTitle->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    }
    shortcutsLayout->addWidget(shortcutsTitle);

    auto* shortcutsGrid = new QGridLayout;
    shortcutsGrid->setOriginCorner(AppLanguage::isPersian() ? Qt::TopRightCorner
                                                            : Qt::TopLeftCorner);
    shortcutsGrid->setContentsMargins(0, 0, 0, 0);
    shortcutsGrid->setHorizontalSpacing(18);
    shortcutsGrid->setVerticalSpacing(8);
    shortcutsGrid->setColumnStretch(0, 1);
    shortcutsGrid->setColumnStretch(1, 0);

    addShortcutRow(shortcutsGrid, 0, tr("Add new patient"), QStringLiteral("Ctrl+N"));
    addShortcutRow(shortcutsGrid, 1, tr("Edit selected patient"), QStringLiteral("F2 / Enter"));
    addShortcutRow(shortcutsGrid, 2, tr("Delete selected patient"), QStringLiteral("Delete"));
    addShortcutRow(shortcutsGrid, 3, tr("Go to search"), QStringLiteral("Ctrl+F"));
    shortcutsLayout->addLayout(shortcutsGrid);
    root->addWidget(shortcutsFrame);

    root->addStretch(1);

    auto* btns = new QHBoxLayout;
    btns->addStretch(1);
    auto* ok = new QPushButton(tr("Close"));
    ok->setDefault(true);
    btns->addWidget(ok);
    btns->addStretch(1);
    root->addLayout(btns);

    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace DentalPatients
