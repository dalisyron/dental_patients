#include "core/AppLanguage.h"

#include "Version.h"
#include "core/PersianText.h"

#include <QApplication>
#include <QHash>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace DentalPatients::AppLanguage {

namespace {

constexpr auto kSettingsKey = "ui/language";

// Source strings are English; this table carries the complete Persian UI.
// Lookup is by source text only — the few strings shared across contexts
// ("Error", "Cancel", ...) translate identically everywhere.
const QHash<QString, QString>& faStrings() {
    static const QHash<QString, QString> map = {
        // Shared
        {QStringLiteral("OK"), QStringLiteral("تأیید")},
        {QStringLiteral("Cancel"), QStringLiteral("انصراف")},
        {QStringLiteral("Error"), QStringLiteral("خطا")},
        {QStringLiteral("Warning"), QStringLiteral("هشدار")},
        {QStringLiteral("Yes"), QStringLiteral("بله")},
        {QStringLiteral("No"), QStringLiteral("خیر")},
        {QStringLiteral("Close"), QStringLiteral("بستن")},
        {QStringLiteral("Restore"), QStringLiteral("بازگردانی")},
        {QStringLiteral("Database error"), QStringLiteral("خطای پایگاه داده")},
        {QStringLiteral("Restore error"), QStringLiteral("خطای بازگردانی")},
        {QStringLiteral("Restore failed:\n%1"), QStringLiteral("بازگردانی با خطا مواجه شد:\n%1")},
        {QStringLiteral("Family name"), QStringLiteral("نام خانوادگی")},
        {QStringLiteral("Given name"), QStringLiteral("نام")},
        {QStringLiteral("Case number"), QStringLiteral("شماره پرونده")},
        {QStringLiteral("Phone"), QStringLiteral("تلفن")},
        {QStringLiteral("Notes"), QStringLiteral("یادداشت")},

        // main.cpp
        {QStringLiteral("Invalid backup file"), QStringLiteral("فایل پشتیبان نامعتبر")},
        {QStringLiteral("The database could not be opened and the selected backup file is unreadable:\n%1\n\nDatabase error:\n%2"),
         QStringLiteral("پایگاه داده باز نشد و فایل پشتیبان انتخاب‌شده نیز قابل خواندن نیست:\n%1\n\nخطای پایگاه داده:\n%2")},
        {QStringLiteral("The database is damaged:\n%1\n\nRestore your data from the selected backup file?"),
         QStringLiteral("پایگاه داده آسیب دیده است:\n%1\n\nآیا می‌خواهید اطلاعات از فایل پشتیبان انتخاب‌شده بازگردانی شود؟")},
        {QStringLiteral("The database is damaged:\n%1\n\nRestore from the most recent backup (%2)?"),
         QStringLiteral("پایگاه داده آسیب دیده است:\n%1\n\nآیا مایل به بازگردانی از آخرین پشتیبان (%2) هستید؟")},
        {QStringLiteral("The database could not be opened:\n%1"),
         QStringLiteral("بازکردن پایگاه داده ممکن نیست:\n%1")},

        // MainWindow
        {QStringLiteral("Dental Patients backup files (*.dpbackup)"),
         QStringLiteral("فایل پشتیبان Dental Patients (*.dpbackup)")},
        {QStringLiteral("Search name, case number, phone..."),
         QStringLiteral("جستجوی نام، شماره پرونده، تلفن...")},
        {QStringLiteral("Add Patient"), QStringLiteral("افزودن بیمار")},
        {QStringLiteral("Initial patient data setup"), QStringLiteral("راه‌اندازی اولیه اطلاعات بیماران")},
        {QStringLiteral("To get started, load a Dental Patients backup file (.dpbackup) or start with an empty database."),
         QStringLiteral("برای شروع، فایل پشتیبان Dental Patients با پسوند .dpbackup را بارگذاری کنید یا پایگاه داده خالی بسازید.")},
        {QStringLiteral("Load backup file (.dpbackup)"), QStringLiteral("بارگذاری فایل پشتیبان (.dpbackup)")},
        {QStringLiteral("Start with an empty database"), QStringLiteral("شروع با پایگاه داده خالی")},
        {QStringLiteral("&File"), QStringLiteral("&پرونده")},
        {QStringLiteral("Add new patient..."), QStringLiteral("افزودن بیمار جدید...")},
        {QStringLiteral("Edit selected patient..."), QStringLiteral("ویرایش بیمار انتخاب‌شده...")},
        {QStringLiteral("Delete selected patient..."), QStringLiteral("حذف بیمار انتخاب‌شده...")},
        {QStringLiteral("Export CSV..."), QStringLiteral("ذخیره خروجی CSV...")},
        {QStringLiteral("Quit"), QStringLiteral("خروج")},
        {QStringLiteral("&Tools"), QStringLiteral("&ابزارها")},
        {QStringLiteral("Create backup"), QStringLiteral("ایجاد پشتیبان")},
        {QStringLiteral("Restore from backup..."), QStringLiteral("بازگردانی از پشتیبان...")},
        {QStringLiteral("Recycle bin..."), QStringLiteral("سطل بازیافت...")},
        {QStringLiteral("Show data folder"), QStringLiteral("نمایش پوشه اطلاعات")},
        {QStringLiteral("&Help"), QStringLiteral("&راهنما")},
        {QStringLiteral("About..."), QStringLiteral("درباره برنامه...")},
        {QStringLiteral("&Language"), QStringLiteral("&زبان")},
        {QStringLiteral("Language changed"), QStringLiteral("تغییر زبان")},
        {QStringLiteral("Restart the application now to apply the new language?"),
         QStringLiteral("برای اعمال زبان جدید، برنامه اکنون دوباره راه‌اندازی شود؟")},
        {QStringLiteral("Restart now"), QStringLiteral("راه‌اندازی دوباره")},
        {QStringLiteral("Later"), QStringLiteral("بعداً")},
        {QStringLiteral("Ready for initial setup"), QStringLiteral("آماده راه‌اندازی اولیه")},
        {QStringLiteral("Showing %1 of %2 patients"), QStringLiteral("نمایش %1 از %2 بیمار")},
        {QStringLiteral("Delete patient"), QStringLiteral("حذف بیمار")},
        {QStringLiteral("Delete “%1” (case number %2)?\nDeleted patients can be restored from the recycle bin."),
         QStringLiteral("آیا از حذف «%1» (شماره پرونده %2) مطمئن هستید؟\nبیمار حذف‌شده در سطل بازیافت قابل بازگردانی است.")},
        {QStringLiteral("Delete failed:\n%1"), QStringLiteral("حذف با خطا مواجه شد:\n%1")},
        {QStringLiteral("Export CSV"), QStringLiteral("ذخیره خروجی CSV")},
        {QStringLiteral("Could not open the file for writing:\n%1"),
         QStringLiteral("نتوانستم فایل را برای نوشتن باز کنم:\n%1")},
        {QStringLiteral("Export failed:\n%1"), QStringLiteral("ذخیره خروجی با خطا مواجه شد:\n%1")},
        {QStringLiteral("Export complete"), QStringLiteral("ذخیره موفق")},
        {QStringLiteral("%1 patients were exported."), QStringLiteral("%1 بیمار در فایل ذخیره شد.")},
        {QStringLiteral("Backup failed:\n%1"), QStringLiteral("ایجاد پشتیبان با خطا مواجه شد:\n%1")},
        {QStringLiteral("Backup"), QStringLiteral("پشتیبان")},
        {QStringLiteral("A new backup was created:\n%1"), QStringLiteral("پشتیبان جدید ایجاد شد:\n%1")},
        {QStringLiteral("Select a backup file"), QStringLiteral("انتخاب فایل پشتیبان")},
        {QStringLiteral("Initial setup could not be recorded."),
         QStringLiteral("ثبت راه‌اندازی اولیه با خطا مواجه شد.")},
        {QStringLiteral("This backup file is unreadable:\n%1"),
         QStringLiteral("این فایل پشتیبان قابل خواندن نیست:\n%1")},
        {QStringLiteral("Load initial data"), QStringLiteral("بارگذاری اطلاعات اولیه")},
        {QStringLiteral("Restore backup"), QStringLiteral("بازگردانی پشتیبان")},
        {QStringLiteral("Patient data will be loaded from:\n%1\n\nPatients: %2"),
         QStringLiteral("اطلاعات بیماران از فایل زیر بارگذاری می‌شود:\n%1\n\nتعداد بیماران: %2")},
        {QStringLiteral("Continuing will replace the data on this device with the contents of:\n%1\n\nPatients in the backup: %2\n\nA safety backup of the current data is created before restoring."),
         QStringLiteral("با ادامه، اطلاعات فعلی این دستگاه با اطلاعات فایل زیر جایگزین می‌شود:\n%1\n\nتعداد بیماران در پشتیبان: %2\n\nقبل از بازگردانی، از اطلاعات فعلی یک پشتیبان ایمن ساخته می‌شود.")},
        {QStringLiteral("Load"), QStringLiteral("بارگذاری")},
        {QStringLiteral("Before restoring, creating a safety backup failed:\n%1"),
         QStringLiteral("پیش از بازگردانی، ایجاد پشتیبان ایمن با خطا مواجه شد:\n%1")},
        {QStringLiteral("Restoring the backup failed:\n%1"),
         QStringLiteral("بازگردانی پشتیبان با خطا مواجه شد:\n%1")},
        {QStringLiteral("The data was loaded, but the setup state could not be fully recorded."),
         QStringLiteral("اطلاعات بارگذاری شد، اما ثبت وضعیت راه‌اندازی کامل نشد.")},
        {QStringLiteral("Restore complete"), QStringLiteral("بازگردانی انجام شد")},
        {QStringLiteral("Initial data was loaded successfully."),
         QStringLiteral("اطلاعات اولیه با موفقیت بارگذاری شد.")},
        {QStringLiteral("The backup was restored successfully."),
         QStringLiteral("پشتیبان با موفقیت بازگردانی شد.")},

        // PatientDialog
        {QStringLiteral("Add new patient"), QStringLiteral("افزودن بیمار جدید")},
        {QStringLiteral("Edit patient"), QStringLiteral("ویرایش اطلاعات بیمار")},
        {QStringLiteral("e.g. Smith"), QStringLiteral("مثال: محمدی")},
        {QStringLiteral("Family name *"), QStringLiteral("نام خانوادگی *")},
        {QStringLiteral("e.g. Sarah"), QStringLiteral("مثال: علی")},
        {QStringLiteral("Given name *"), QStringLiteral("نام *")},
        {QStringLiteral("e.g. 1234"), QStringLiteral("مثال: 1234")},
        {QStringLiteral("Auto"), QStringLiteral("خودکار")},
        {QStringLiteral("Picks the first free case number starting at 6000."),
         QStringLiteral("اولین شماره پرونده آزاد از ۶۰۰۰ انتخاب می‌شود.")},
        {QStringLiteral("Case number *"), QStringLiteral("شماره پرونده *")},
        {QStringLiteral("Optional"), QStringLiteral("اختیاری")},
        {QStringLiteral("Save"), QStringLiteral("ذخیره")},
        {QStringLiteral("Family name is required."), QStringLiteral("نام خانوادگی الزامی است.")},
        {QStringLiteral("Given name is required."), QStringLiteral("نام الزامی است.")},
        {QStringLiteral("Case number is required."), QStringLiteral("شماره پرونده الزامی است.")},
        {QStringLiteral("Finding a free case number failed:\n%1"),
         QStringLiteral("محاسبه شماره پرونده آزاد با خطا مواجه شد:\n%1")},
        {QStringLiteral("Suggested number: %1"), QStringLiteral("شماره پیشنهادی: %1")},
        {QStringLiteral("Duplicate case number"), QStringLiteral("شماره پرونده تکراری")},
        {QStringLiteral("This case number is already used by “%1”.\nSave with the same case number anyway?"),
         QStringLiteral("این شماره پرونده قبلاً برای بیمار «%1» ثبت شده است.\nآیا می‌خواهید با همین شماره پرونده ذخیره شود؟")},
        {QStringLiteral("Saving failed:\n%1"), QStringLiteral("ذخیره با خطا مواجه شد:\n%1")},

        // TrashDialog
        {QStringLiteral("Recycle bin"), QStringLiteral("سطل بازیافت")},
        {QStringLiteral("Deleted patients are kept here and can be restored when needed."),
         QStringLiteral("بیماران حذف‌شده در اینجا نگه‌داری می‌شوند تا در صورت نیاز بازگردانده شوند.")},
        {QStringLiteral("Please select a row."), QStringLiteral("لطفاً یک ردیف را انتخاب کنید.")},

        // AboutDialog
        {QStringLiteral("About"), QStringLiteral("درباره برنامه")},
        {QStringLiteral("Version %1"), QStringLiteral("نسخه %1")},
        {QStringLiteral("Created by Mobin Dariush"), QStringLiteral("ساخته شده توسط: مبین داریوش")},
        {QStringLiteral("Offline dental patient records manager.\nAll data stays on this computer; no internet is required."),
         QStringLiteral("نرم‌افزار مدیریت پرونده بیماران دندانپزشکی.\nتمام اطلاعات روی همین رایانه نگه‌داری می‌شود و نیازی به اینترنت ندارد.")},
        {QStringLiteral("Keyboard shortcuts"), QStringLiteral("میانبرهای صفحه‌کلید")},
        {QStringLiteral("Add new patient"), QStringLiteral("افزودن بیمار جدید")},
        {QStringLiteral("Edit selected patient"), QStringLiteral("ویرایش بیمار انتخاب‌شده")},
        {QStringLiteral("Delete selected patient"), QStringLiteral("حذف بیمار انتخاب‌شده")},
        {QStringLiteral("Go to search"), QStringLiteral("رفتن به جستجو")},
    };
    return map;
}

// Qt's own standard-button labels (QMessageBox Yes/No etc.) resolve through
// the QPlatformTheme context; without qtbase's .qm files we translate the
// handful the app actually shows.
QString platformThemeFa(QString source) {
    source.remove(QLatin1Char('&'));
    static const QHash<QString, QString> map = {
        {QStringLiteral("OK"), QStringLiteral("تأیید")},
        {QStringLiteral("Cancel"), QStringLiteral("انصراف")},
        {QStringLiteral("Yes"), QStringLiteral("بله")},
        {QStringLiteral("No"), QStringLiteral("خیر")},
        {QStringLiteral("Close"), QStringLiteral("بستن")},
    };
    return map.value(source);
}

class FaTranslator final : public QTranslator {
public:
    using QTranslator::QTranslator;

    bool isEmpty() const override { return false; }

    QString translate(const char* context, const char* sourceText,
                      const char* disambiguation, int n) const override {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        const QString source = QString::fromUtf8(sourceText);
        if (qstrcmp(context, "QPlatformTheme") == 0
            || qstrcmp(context, "QMessageBox") == 0
            || qstrcmp(context, "QDialogButtonBox") == 0) {
            return platformThemeFa(source);
        }
        return faStrings().value(source);
    }
};

Language readStoredLanguage() {
    const QSettings settings;
    const QString code = settings.value(QLatin1String(kSettingsKey),
                                        QStringLiteral("en")).toString();
    return code == QLatin1String("fa") ? Language::Persian : Language::English;
}

} // namespace

Language current() {
    return readStoredLanguage();
}

void setCurrent(Language lang) {
    QSettings settings;
    settings.setValue(QLatin1String(kSettingsKey),
                      lang == Language::Persian ? QStringLiteral("fa")
                                                : QStringLiteral("en"));
}

namespace {
// -1 unset, else Language. The language is fixed for the lifetime of the
// process (a restart applies changes), so the first read is cached here.
int g_cachedPersian = -1;
} // namespace

bool isPersian() {
    if (g_cachedPersian < 0) {
        g_cachedPersian = readStoredLanguage() == Language::Persian ? 1 : 0;
    }
    return g_cachedPersian == 1;
}

void overrideForTesting(Language lang) {
    g_cachedPersian = lang == Language::Persian ? 1 : 0;
}

QString appDisplayName() {
    return QString::fromUtf8(isPersian() ? Version::kAppNameFa : Version::kAppName);
}

QString localizeDigits(const QString& input) {
    return isPersian() ? PersianText::toPersianDigits(input) : input;
}

void applyToApplication(QApplication& app) {
    if (isPersian()) {
        QLocale::setDefault(QLocale(QLocale::Persian, QLocale::Iran));
        QApplication::setLayoutDirection(Qt::RightToLeft);
        static FaTranslator translator;
        app.installTranslator(&translator);
    } else {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        QApplication::setLayoutDirection(Qt::LeftToRight);
    }
    QApplication::setApplicationDisplayName(appDisplayName());
}

} // namespace DentalPatients::AppLanguage
