#include "Version.h"
#include "db/Database.h"
#include "db/PatientRepository.h"
#include "db/CsvImporter.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStringList>

using namespace DentalPatients;

namespace {

void installPersianFont() {
    // Load every Vazirmatn weight bundled into the app resources, then make
    // the regular weight the application-wide default font.
    const QStringList resources = {
        QStringLiteral(":/fonts/Vazirmatn-Regular.ttf"),
        QStringLiteral(":/fonts/Vazirmatn-Medium.ttf"),
        QStringLiteral(":/fonts/Vazirmatn-Bold.ttf"),
    };
    QString family;
    for (const auto& r : resources) {
        const int id = QFontDatabase::addApplicationFont(r);
        if (id < 0) continue;
        const auto fams = QFontDatabase::applicationFontFamilies(id);
        if (!fams.isEmpty()) family = fams.first();
    }
    if (!family.isEmpty()) {
        QFont f(family, 11);
        QApplication::setFont(f);
    }
}

void applyStylesheet(QApplication& app) {
    QFile f(QStringLiteral(":/styles/app.qss"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(f.readAll()));
    }
}

bool firstRunCsvImport(PatientRepository& repo) {
    if (repo.count() > 0) return true;                // not first run
    if (!repo.meta(QStringLiteral("csv_import_done")).isEmpty()) return true;

    // Look for the seed CSV next to the executable, then in the app bundle.
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("patient_list_merged_sorted.csv")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../patient_list_merged_sorted.csv")),
    };
    QString seed;
    for (const auto& c : candidates) {
        if (QFileInfo::exists(c)) { seed = c; break; }
    }
    if (seed.isEmpty()) return true;                  // user has none, that's fine

    auto result = CsvImporter::importFromFile(seed, repo);
    if (!result.ok) {
        QMessageBox::warning(nullptr,
            QObject::tr("خطای ورود اطلاعات"),
            QObject::tr("ورود فایل CSV اولیه با خطا مواجه شد:\n%1").arg(result.error));
        return false;
    }
    repo.setMeta(QStringLiteral("csv_import_done"),
                 QString::number(QDateTime::currentSecsSinceEpoch()));
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    // High-DPI scaling is on by default in Qt 6.
    QApplication app(argc, argv);

    QApplication::setApplicationName(QString::fromUtf8(Version::kAppName));
    QApplication::setApplicationDisplayName(QString::fromUtf8(Version::kAppNameFa));
    QApplication::setApplicationVersion(QString::fromUtf8(Version::kString));
    QApplication::setOrganizationName(QString::fromUtf8(Version::kPublisher));
    QLocale::setDefault(QLocale(QLocale::Persian, QLocale::Iran));
    QApplication::setLayoutDirection(Qt::RightToLeft);

    installPersianFont();
    applyStylesheet(app);

    // ---- Open DB (with auto-restore from latest backup if corrupt) ---------
    QString openErr;
    if (!Database::instance().open(&openErr)) {
        // Try to restore from the most recent backup.
        const auto backups = Database::instance().listBackups();
        if (!backups.isEmpty()) {
            const auto reply = QMessageBox::question(nullptr,
                QObject::tr("خطای پایگاه داده"),
                QObject::tr("پایگاه داده آسیب دیده است:\n%1\n\n"
                            "آیا مایل به بازگردانی از آخرین پشتیبان (%2) هستید؟")
                    .arg(openErr, QFileInfo(backups.first()).fileName()),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                QString restoreErr;
                if (!Database::instance().restoreFromBackup(backups.first(), &restoreErr)) {
                    QMessageBox::critical(nullptr,
                        QObject::tr("خطای بازگردانی"),
                        QObject::tr("بازگردانی با خطا مواجه شد:\n%1").arg(restoreErr));
                    return 2;
                }
            } else {
                return 2;
            }
        } else {
            QMessageBox::critical(nullptr,
                QObject::tr("خطای پایگاه داده"),
                QObject::tr("بازکردن پایگاه داده ممکن نیست:\n%1").arg(openErr));
            return 2;
        }
    }

    PatientRepository repo(Database::instance().sql());
    firstRunCsvImport(repo);

    // Daily backup after the first-run import, so a fresh install backs up the
    // seeded patient data instead of an empty database.
    QString backupErr;
    Database::instance().backupTodayIfMissing(&backupErr);

    MainWindow w(&repo);
    w.show();

    const int rc = app.exec();
    Database::instance().close();
    return rc;
}
