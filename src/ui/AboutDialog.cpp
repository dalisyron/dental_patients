#include "ui/AboutDialog.h"

#include "Version.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace DentalPatients {

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("درباره برنامه"));
    setModal(true);
    setLayoutDirection(Qt::RightToLeft);
    resize(420, 240);

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

    auto* desc = new QLabel(tr(
        "نرم‌افزار مدیریت پرونده بیماران دندانپزشکی.\n"
        "تمام اطلاعات روی همین رایانه نگه‌داری می‌شود و نیازی به اینترنت ندارد."));
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    root->addWidget(desc);

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
