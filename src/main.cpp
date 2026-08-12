#include "Version.h"
#include "core/AppLanguage.h"
#include "core/SingleInstance.h"
#include "db/Database.h"
#include "db/PatientRepository.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSize>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QWidget>

using namespace DentalPatients;

namespace {

void installApplicationFont() {
    // Load every Vazirmatn weight bundled into the app resources, then make
    // the regular weight the application-wide default font. Vazirmatn covers
    // both the Latin and the Persian UI.
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

void setApplicationIcon(QApplication& app) {
    QIcon icon;
    const int sizes[] = {16, 24, 32, 64, 128, 256};
    for (const int size : sizes) {
        icon.addFile(QStringLiteral(":/icons/dental-record-%1x%1.png").arg(size),
                     QSize(size, size));
    }
    if (!icon.isNull()) {
        app.setWindowIcon(icon);
    }
}

QString startupBackupPath(const QStringList& args) {
    for (int i = 1; i < args.size(); ++i) {
        const QFileInfo info(args.at(i));
        if (info.suffix().compare(QStringLiteral("dpbackup"), Qt::CaseInsensitive) == 0) {
            return info.absoluteFilePath();
        }
    }
    return {};
}

void showCritical(QWidget* parent, const QString& title, const QString& text) {
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(title);
    box.setText(text);
    auto* okButton = box.addButton(QObject::tr("OK"), QMessageBox::AcceptRole);
    box.setDefaultButton(okButton);
    box.setEscapeButton(okButton);
    box.exec();
}

bool offerRestoreFromSelectedBackup(const QString& backupPath, const QString& openError) {
    Database::BackupInfo info;
    QString inspectErr;
    if (!Database::inspectBackup(backupPath, &info, &inspectErr)) {
        showCritical(nullptr,
            QObject::tr("Invalid backup file"),
            QObject::tr("The database could not be opened and the selected backup file is unreadable:\n%1\n\nDatabase error:\n%2")
                .arg(inspectErr, openError));
        return false;
    }

    const auto reply = QMessageBox::question(nullptr,
        QObject::tr("Database error"),
        QObject::tr("The database is damaged:\n%1\n\nRestore your data from the selected backup file?")
            .arg(openError),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return false;

    QString restoreErr;
    if (!Database::instance().restoreFromBackup(backupPath, &restoreErr)) {
        showCritical(nullptr,
            QObject::tr("Restore error"),
            QObject::tr("Restore failed:\n%1").arg(restoreErr));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    // High-DPI scaling is on by default in Qt 6.
    QApplication app(argc, argv);

    QApplication::setApplicationName(QString::fromUtf8(Version::kAppName));
    QApplication::setApplicationVersion(QString::fromUtf8(Version::kString));
    QApplication::setOrganizationName(QString::fromUtf8(Version::kPublisher));

    // Locale, layout direction, display name, and the Persian translator all
    // derive from the persisted language (default: English).
    AppLanguage::applyToApplication(app);

    // Language-switch relaunch: give the exiting instance time to release the
    // single-instance server and close the database before we acquire them.
    if (QApplication::arguments().contains(QStringLiteral("--relaunched"))) {
        QThread::msleep(1200);
    }

    setApplicationIcon(app);
    installApplicationFont();
    applyStylesheet(app);

    SingleInstance singleInstance;
    if (!singleInstance.tryAcquire(QApplication::arguments())) {
        return 0;
    }

    const QString requestedBackup = startupBackupPath(QApplication::arguments());
    bool requestedBackupAlreadyHandled = false;

    // ---- Open DB (with auto-restore from latest backup if corrupt) ---------
    QString openErr;
    if (!Database::instance().open(&openErr)) {
        if (!requestedBackup.isEmpty()) {
            if (!offerRestoreFromSelectedBackup(requestedBackup, openErr)) {
                return 2;
            }
            requestedBackupAlreadyHandled = true;
        } else {
            // Try to restore from the most recent backup.
            const auto backups = Database::instance().listBackups();
            if (!backups.isEmpty()) {
                const auto reply = QMessageBox::question(nullptr,
                    QObject::tr("Database error"),
                    QObject::tr("The database is damaged:\n%1\n\nRestore from the most recent backup (%2)?")
                        .arg(openErr, QFileInfo(backups.first()).fileName()),
                    QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    QString restoreErr;
                    if (!Database::instance().restoreFromBackup(backups.first(), &restoreErr)) {
                        showCritical(nullptr,
                            QObject::tr("Restore error"),
                            QObject::tr("Restore failed:\n%1").arg(restoreErr));
                        return 2;
                    }
                } else {
                    return 2;
                }
            } else {
                showCritical(nullptr,
                    QObject::tr("Database error"),
                    QObject::tr("The database could not be opened:\n%1").arg(openErr));
                return 2;
            }
        }
    }

    PatientRepository repo(Database::instance().sql());
    if (requestedBackupAlreadyHandled && !repo.isInitialized()) {
        repo.markInitialized();
    }

    // Do not back up a newly-created, uninitialized database. After the user
    // loads a .dpbackup or explicitly starts empty, normal daily backups resume.
    if (repo.isInitialized()) {
        QString backupErr;
        Database::instance().backupTodayIfMissing(&backupErr);
    }

    MainWindow w(&repo);
    w.show();

    QObject::connect(&singleInstance, &SingleInstance::secondInstanceLaunched,
                     &w, &MainWindow::onSecondInstanceLaunched);

    if (!requestedBackup.isEmpty() && !requestedBackupAlreadyHandled) {
        QTimer::singleShot(0, &w, [requestedBackup, &w] {
            w.openBackupFile(requestedBackup);
        });
    }

    const int rc = app.exec();
    Database::instance().close();
    return rc;
}
