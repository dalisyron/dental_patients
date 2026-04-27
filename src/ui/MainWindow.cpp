#include "ui/MainWindow.h"

#include "Version.h"
#include "core/PersianText.h"
#include "db/Database.h"
#include "db/PatientRepository.h"
#include "ui/AboutDialog.h"
#include "ui/PatientDialog.h"
#include "ui/PatientTableModel.h"
#include "ui/TrashDialog.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileDevice>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QSize>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTextStream>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace DentalPatients {

namespace {

constexpr int kSearchDebounceMs = 120;     // snappy on a slow CPU
constexpr int kFirstRunTextMaxWidth = 520;

QString backupFileFilter() {
    return MainWindow::tr("فایل پشتیبان Dental Patients (*.dpbackup)");
}

QIcon createAddIcon() {
    constexpr int kSize = 18;
    constexpr qreal kInset = 4.5;

    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 2.6, Qt::SolidLine, Qt::RoundCap));

    const qreal center = kSize / 2.0;
    painter.drawLine(QPointF(center, kInset), QPointF(center, kSize - kInset));
    painter.drawLine(QPointF(kInset, center), QPointF(kSize - kInset, center));

    return QIcon(pixmap);
}

QString csvEscape(const QString& s) {
    QString out = s;
    const bool needsQuote = out.contains(QLatin1Char(','))
                          || out.contains(QLatin1Char('"'))
                          || out.contains(QLatin1Char('\n'))
                          || out.contains(QLatin1Char('\r'));
    if (needsQuote) {
        out.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + out + QLatin1Char('"');
    }
    return out;
}

PatientDialog::Field dialogFieldForColumn(int column) {
    switch (column) {
        case PatientTableModel::Col_GivenName:  return PatientDialog::Field::GivenName;
        case PatientTableModel::Col_FileNumber: return PatientDialog::Field::FileNumber;
        case PatientTableModel::Col_Phone:      return PatientDialog::Field::Phone;
        case PatientTableModel::Col_Notes:      return PatientDialog::Field::Notes;
        case PatientTableModel::Col_FamilyName:
        default:                                return PatientDialog::Field::FamilyName;
    }
}

} // namespace

MainWindow::MainWindow(PatientRepository* repo, QWidget* parent)
    : QMainWindow(parent), m_repo(repo) {
    setWindowTitle(QStringLiteral("%1  -  v%2")
                       .arg(QString::fromUtf8(Version::kAppNameFa),
                            QString::fromUtf8(Version::kString)));
    resize(1100, 720);
    setLayoutDirection(Qt::RightToLeft);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(kSearchDebounceMs);
    connect(&m_searchDebounce, &QTimer::timeout, this, [this]{
        refreshTable(m_pendingQuery);
    });

    buildUi();
    buildMenus();
    refreshTable();
    selectFirstRow();
    m_search->setFocus();
}

void MainWindow::buildUi() {
    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(10);

    m_search = new QLineEdit;
    m_search->setObjectName(QStringLiteral("searchBox"));
    m_search->setPlaceholderText(tr("جستجوی نام، شماره پرونده، تلفن..."));
    m_search->setClearButtonEnabled(true);
    m_search->setLayoutDirection(Qt::RightToLeft);
    m_search->setAlignment(Qt::AlignRight | Qt::AlignAbsolute | Qt::AlignVCenter);
    m_search->setCursorMoveStyle(Qt::LogicalMoveStyle);
    m_search->setMinimumHeight(36);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);

    m_addButton = new QPushButton(tr("افزودن بیمار"));
    m_addButton->setIcon(createAddIcon());
    m_addButton->setIconSize(QSize(18, 18));
    m_addButton->setObjectName(QStringLiteral("primaryButton"));
    m_addButton->setMinimumHeight(36);
    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::onAddClicked);

    topBar->addWidget(m_search, 1);
    topBar->addWidget(m_addButton, 0);
    root->addLayout(topBar);

    m_model = new PatientTableModel(this);
    m_table = new QTableView;
    m_table->setObjectName(QStringLiteral("patientsTable"));
    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setShowGrid(false);

    auto* hh = m_table->horizontalHeader();
    hh->setSectionsClickable(true);
    hh->setSortIndicatorShown(true);
    hh->setSortIndicator(PatientTableModel::Col_FamilyName, Qt::AscendingOrder);
    hh->setSectionResizeMode(PatientTableModel::Col_FamilyName, QHeaderView::Stretch);
    hh->setSectionResizeMode(PatientTableModel::Col_GivenName,  QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(PatientTableModel::Col_FileNumber, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(PatientTableModel::Col_Phone,      QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(PatientTableModel::Col_Notes,      QHeaderView::Stretch);
    connect(hh, &QHeaderView::sectionClicked, this, &MainWindow::onHeaderClicked);

    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        if (index.isValid()) {
            m_table->setCurrentIndex(index);
        }
        editCurrent(index.column());
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]{ updatePatientActions(); });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this]{ updatePatientActions(); });

    m_initialSetupWidget = buildInitialSetupWidget();
    m_contentStack = new QStackedWidget;
    m_contentStack->addWidget(m_table);
    m_contentStack->addWidget(m_initialSetupWidget);
    root->addWidget(m_contentStack, 1);

    setCentralWidget(central);

    m_statusLabel = new QLabel;
    statusBar()->addPermanentWidget(m_statusLabel);

    // Keyboard shortcuts that work regardless of focus.
    auto* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    enterShortcut->setContext(Qt::ApplicationShortcut);
    connect(enterShortcut, &QShortcut::activated, this, [this]{
        if (m_search->hasFocus() && m_model->patientCount() > 0) {
            selectFirstRow();
            m_table->setFocus();
        } else if (m_table->currentIndex().isValid()) {
            onEditCurrent();
        }
    });
    auto* delShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), m_table);
    connect(delShortcut, &QShortcut::activated, this, &MainWindow::onDeleteCurrent);
    auto* findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(findShortcut, &QShortcut::activated, this, [this]{ m_search->setFocus(); m_search->selectAll(); });
    auto* newShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this);
    connect(newShortcut, &QShortcut::activated, this, &MainWindow::onAddClicked);
}

QWidget* MainWindow::buildInitialSetupWidget() {
    auto* widget = new QWidget;
    widget->setObjectName(QStringLiteral("initialSetupView"));

    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->addStretch(1);

    auto* content = new QVBoxLayout;
    content->setSpacing(14);
    content->setAlignment(Qt::AlignHCenter);

    auto* title = new QLabel(tr("راه‌اندازی اولیه اطلاعات بیماران"));
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* body = new QLabel(tr("برای شروع، فایل پشتیبان Dental Patients با پسوند .dpbackup را بارگذاری کنید یا پایگاه داده خالی بسازید."));
    body->setAlignment(Qt::AlignCenter);
    body->setWordWrap(true);
    body->setMaximumWidth(kFirstRunTextMaxWidth);

    auto* loadBackup = new QPushButton(tr("بارگذاری فایل پشتیبان (.dpbackup)"));
    loadBackup->setObjectName(QStringLiteral("primaryButton"));
    loadBackup->setMinimumHeight(38);
    loadBackup->setMinimumWidth(260);
    connect(loadBackup, &QPushButton::clicked, this, &MainWindow::onLoadInitialBackup);

    auto* startEmpty = new QPushButton(tr("شروع با پایگاه داده خالی"));
    startEmpty->setMinimumHeight(34);
    startEmpty->setMinimumWidth(260);
    connect(startEmpty, &QPushButton::clicked, this, &MainWindow::onStartEmptyDatabase);

    content->addWidget(title);
    content->addWidget(body);
    content->addSpacing(8);
    content->addWidget(loadBackup, 0, Qt::AlignHCenter);
    content->addWidget(startEmpty, 0, Qt::AlignHCenter);

    outer->addLayout(content);
    outer->addStretch(1);
    return widget;
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(tr("&پرونده"));

    m_actAdd = file->addAction(tr("افزودن بیمار جدید..."), QKeySequence::New, this, &MainWindow::onAddClicked);
    m_actEdit = file->addAction(tr("ویرایش بیمار انتخاب‌شده..."), this, &MainWindow::onEditCurrent);
    m_actDelete = file->addAction(tr("حذف بیمار انتخاب‌شده..."), this, &MainWindow::onDeleteCurrent);
    updatePatientActions();
    file->addSeparator();
    m_actExport = file->addAction(tr("ذخیره خروجی CSV..."), this, &MainWindow::onExportCsv);
    file->addSeparator();
    m_actQuit = file->addAction(tr("خروج"), QKeySequence::Quit, this, &QWidget::close);

    auto* tools = menuBar()->addMenu(tr("&ابزارها"));
    m_actBackup = tools->addAction(tr("ایجاد پشتیبان"), this, &MainWindow::onBackupNow);
    m_actRestore = tools->addAction(tr("بازگردانی از پشتیبان..."), this, &MainWindow::onRestoreBackup);
    m_actTrash  = tools->addAction(tr("سطل بازیافت..."), this, &MainWindow::onShowTrash);
    tools->addSeparator();
    m_actDataLoc = tools->addAction(tr("نمایش پوشه اطلاعات"), this, &MainWindow::onShowDataLocation);

    auto* help = menuBar()->addMenu(tr("&راهنما"));
    m_actAbout = help->addAction(tr("درباره برنامه..."), this, &MainWindow::onAbout);
}

void MainWindow::onSearchChanged(const QString& text) {
    m_pendingQuery = text;
    m_searchDebounce.start();
}

void MainWindow::onHeaderClicked(int section) {
    if (section != PatientTableModel::Col_FamilyName &&
        section != PatientTableModel::Col_FileNumber) {
        return;
    }

    if (m_sortColumn == section) {
        m_sortOrder = (m_sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        m_sortColumn = section;
        m_sortOrder = Qt::AscendingOrder;
    }
    m_table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
    refreshTable(m_search->text());
    selectFirstRow();
}

PatientRepository::SortField MainWindow::currentSortField() const {
    if (m_sortColumn == PatientTableModel::Col_FileNumber) {
        return PatientRepository::SortField::FileNumber;
    }
    return PatientRepository::SortField::FamilyName;
}

bool MainWindow::isInitialSetupPending() const {
    return m_repo && !m_repo->isInitialized();
}

void MainWindow::refreshTable(const QString& query) {
    const int total = m_repo->count();
    auto patients = m_repo->search(query, total,
                                   currentSortField(),
                                   m_sortOrder == Qt::AscendingOrder);
    m_model->setPatients(std::move(patients));
    updateInitialSetupState(total);
    updatePatientActions();
}

void MainWindow::updateInitialSetupState(int total) {
    const bool pending = isInitialSetupPending();
    if (m_contentStack) {
        m_contentStack->setCurrentWidget(pending ? m_initialSetupWidget : static_cast<QWidget*>(m_table));
    }
    if (m_search) {
        m_search->setEnabled(!pending);
    }
    if (m_addButton) {
        m_addButton->setEnabled(!pending);
    }
    if (m_actAdd) {
        m_actAdd->setEnabled(!pending);
    }
    if (pending && m_statusLabel) {
        m_statusLabel->setText(tr("آماده راه‌اندازی اولیه"));
    } else {
        updateStatus(total);
    }
}

void MainWindow::selectFirstRow() {
    if (m_model->patientCount() > 0) {
        m_table->selectRow(0);
    }
    updatePatientActions();
}

void MainWindow::updatePatientActions() {
    const QModelIndex idx = m_table ? m_table->currentIndex() : QModelIndex{};
    const bool hasPatient = m_table && m_table->selectionModel()
                         && m_table->selectionModel()->hasSelection()
                         && idx.isValid()
                         && m_model
                         && idx.row() >= 0
                         && idx.row() < m_model->patientCount()
                         && m_model->patientAt(idx.row()).id >= 0;
    if (m_actEdit) {
        m_actEdit->setEnabled(hasPatient);
    }
    if (m_actDelete) {
        m_actDelete->setEnabled(hasPatient);
    }
}

void MainWindow::updateStatus(int total) {
    const int shown = m_model->patientCount();
    const QString msg = tr("نمایش %1 از %2 بیمار")
                            .arg(PersianText::toPersianDigits(QString::number(shown)),
                                 PersianText::toPersianDigits(QString::number(total)));
    m_statusLabel->setText(msg);
}

void MainWindow::onAddClicked() {
    if (isInitialSetupPending()) return;

    PatientDialog dlg(PatientDialog::Mode::Add, m_repo, Patient{}, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshTable(m_search->text());
        // Select the newly added patient.
        const auto added = dlg.result();
        for (int i = 0; i < m_model->patientCount(); ++i) {
            if (m_model->patientAt(i).id == added.id) {
                m_table->selectRow(i);
                m_table->scrollTo(m_model->index(i, 0));
                break;
            }
        }
    }
}

void MainWindow::onEditCurrent() {
    editCurrent(PatientTableModel::Col_FamilyName);
}

void MainWindow::editCurrent(int initialFocusColumn) {
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid()) return;
    const Patient p = m_model->patientAt(idx.row());
    if (p.id < 0) return;
    PatientDialog dlg(PatientDialog::Mode::Edit, m_repo, p,
                      dialogFieldForColumn(initialFocusColumn), this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshTable(m_search->text());
        for (int i = 0; i < m_model->patientCount(); ++i) {
            if (m_model->patientAt(i).id == p.id) { m_table->selectRow(i); break; }
        }
    }
}

void MainWindow::onDeleteCurrent() {
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid()) return;
    const Patient p = m_model->patientAt(idx.row());
    if (p.id < 0) return;

    QMessageBox confirm(this);
    confirm.setIcon(QMessageBox::Question);
    confirm.setWindowTitle(tr("حذف بیمار"));
    confirm.setText(tr("آیا از حذف «%1» (شماره پرونده %2) مطمئن هستید؟\n"
                       "بیمار حذف‌شده در سطل بازیافت قابل بازگردانی است.")
                        .arg(p.displayName(), PersianText::toPersianDigits(p.fileNumber)));
    confirm.setLayoutDirection(Qt::RightToLeft);
    auto* yesButton = confirm.addButton(tr("بله"), QMessageBox::YesRole);
    auto* noButton = confirm.addButton(tr("خیر"), QMessageBox::NoRole);
    confirm.setDefaultButton(noButton);
    confirm.setEscapeButton(noButton);
    confirm.exec();

    if (confirm.clickedButton() != yesButton) return;

    QString err;
    if (!m_repo->softDelete(p.id, &err)) {
        QMessageBox::critical(this, tr("خطا"),
            tr("حذف با خطا مواجه شد:\n%1").arg(err));
        return;
    }
    refreshTable(m_search->text());
    updatePatientActions();
}

void MainWindow::onExportCsv() {
    const QString suggested = QStringLiteral("patients-%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("ذخیره خروجی CSV"),
                                                      suggested, tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("خطا"),
            tr("نتوانستم فایل را برای نوشتن باز کنم:\n%1").arg(f.errorString()));
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts.setGenerateByteOrderMark(true);
    ts << "Family Name,Given Name,Patient Name,Case Number,Phone,Notes\n";

    auto all = m_repo->search({}, 1000000, PatientRepository::SortField::FamilyName, true);
    for (const auto& p : all) {
        ts << csvEscape(p.familyName) << ','
           << csvEscape(p.givenName) << ','
           << csvEscape(p.displayName()) << ','
           << csvEscape(p.fileNumber) << ','
           << csvEscape(p.phone) << ','
           << csvEscape(p.notes) << '\n';
    }
    ts.flush();
    if (ts.status() != QTextStream::Ok || !f.flush() || f.error() != QFileDevice::NoError) {
        const QString detail = f.errorString();
        f.close();
        QFile::remove(path);
        QMessageBox::critical(this, tr("خطا"),
            tr("ذخیره خروجی با خطا مواجه شد:\n%1").arg(detail));
        return;
    }
    f.close();
    if (f.error() != QFileDevice::NoError) {
        const QString detail = f.errorString();
        QFile::remove(path);
        QMessageBox::critical(this, tr("خطا"),
            tr("ذخیره خروجی با خطا مواجه شد:\n%1").arg(detail));
        return;
    }
    QMessageBox::information(this, tr("ذخیره موفق"),
        tr("%1 بیمار در فایل ذخیره شد.").arg(PersianText::toPersianDigits(QString::number(all.size()))));
}

void MainWindow::onBackupNow() {
    QString err;
    const QString path = Database::instance().createBackup(&err);
    if (path.isEmpty()) {
        QMessageBox::critical(this, tr("خطا"),
            tr("ایجاد پشتیبان با خطا مواجه شد:\n%1").arg(err));
        return;
    }
    Database::instance().rotateBackups(30);
    QMessageBox::information(this, tr("پشتیبان"),
        tr("پشتیبان جدید ایجاد شد:\n%1").arg(path));
}

void MainWindow::onRestoreBackup() {
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("انتخاب فایل پشتیبان"),
                                                      Database::backupDir(),
                                                      backupFileFilter());
    if (path.isEmpty()) return;
    restoreFromBackupFile(path, false);
}

void MainWindow::onLoadInitialBackup() {
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("انتخاب فایل پشتیبان"),
                                                      QString(),
                                                      backupFileFilter());
    if (path.isEmpty()) return;
    restoreFromBackupFile(path, true);
}

void MainWindow::onStartEmptyDatabase() {
    if (!m_repo->markInitialized()) {
        QMessageBox::critical(this, tr("خطا"),
            tr("ثبت راه‌اندازی اولیه با خطا مواجه شد."));
        return;
    }
    refreshTable();
    m_search->setFocus();
}

void MainWindow::openBackupFile(const QString& path) {
    if (path.isEmpty()) return;
    restoreFromBackupFile(path, isInitialSetupPending());
}

bool MainWindow::restoreFromBackupFile(const QString& path, bool initialLoad) {
    Database::BackupInfo info;
    QString inspectErr;
    if (!Database::inspectBackup(path, &info, &inspectErr)) {
        QMessageBox::critical(this, tr("فایل پشتیبان نامعتبر"),
            tr("این فایل پشتیبان قابل خواندن نیست:\n%1").arg(inspectErr));
        return false;
    }

    const bool initial = initialLoad || isInitialSetupPending();
    const QString patientCount = PersianText::toPersianDigits(QString::number(info.patientCount));
    QMessageBox confirm(this);
    confirm.setIcon(QMessageBox::Question);
    confirm.setLayoutDirection(Qt::RightToLeft);
    confirm.setWindowTitle(initial ? tr("بارگذاری اطلاعات اولیه") : tr("بازگردانی پشتیبان"));
    confirm.setText(initial
        ? tr("اطلاعات بیماران از فایل زیر بارگذاری می‌شود:\n%1\n\nتعداد بیماران: %2")
              .arg(QFileInfo(path).fileName(), patientCount)
        : tr("با ادامه، اطلاعات فعلی این دستگاه با اطلاعات فایل زیر جایگزین می‌شود:\n%1\n\n"
             "تعداد بیماران در پشتیبان: %2\n\n"
             "قبل از بازگردانی، از اطلاعات فعلی یک پشتیبان ایمن ساخته می‌شود.")
              .arg(QFileInfo(path).fileName(), patientCount));
    auto* yesButton = confirm.addButton(initial ? tr("بارگذاری") : tr("بازگردانی"), QMessageBox::AcceptRole);
    auto* noButton = confirm.addButton(tr("انصراف"), QMessageBox::RejectRole);
    confirm.setDefaultButton(noButton);
    confirm.setEscapeButton(noButton);
    confirm.exec();
    if (confirm.clickedButton() != yesButton) return false;

    if (!initial && m_repo->isInitialized()) {
        QString backupErr;
        const QString safetyBackup = Database::instance().createBackup(&backupErr);
        if (safetyBackup.isEmpty()) {
            QMessageBox::critical(this, tr("خطا"),
                tr("پیش از بازگردانی، ایجاد پشتیبان ایمن با خطا مواجه شد:\n%1").arg(backupErr));
            return false;
        }
    }

    QString restoreErr;
    if (!Database::instance().restoreFromBackup(path, &restoreErr)) {
        if (!Database::instance().isOpen()) {
            QString reopenErr;
            Database::instance().open(&reopenErr);
        }
        if (Database::instance().isOpen()) {
            m_repo->resetDatabase(Database::instance().sql());
        }
        QMessageBox::critical(this, tr("خطای بازگردانی"),
            tr("بازگردانی پشتیبان با خطا مواجه شد:\n%1").arg(restoreErr));
        return false;
    }

    m_repo->resetDatabase(Database::instance().sql());
    if (!m_repo->markInitialized()) {
        QMessageBox::warning(this, tr("هشدار"),
            tr("اطلاعات بارگذاری شد، اما ثبت وضعیت راه‌اندازی کامل نشد."));
    }
    refreshTable();
    selectFirstRow();
    QMessageBox::information(this, tr("بازگردانی انجام شد"),
        initial ? tr("اطلاعات اولیه با موفقیت بارگذاری شد.")
                : tr("پشتیبان با موفقیت بازگردانی شد."));
    return true;
}

void MainWindow::onShowTrash() {
    TrashDialog dlg(m_repo, this);
    connect(&dlg, &TrashDialog::restored, this, [this]{ refreshTable(m_search->text()); });
    dlg.exec();
}

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::onShowDataLocation() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(Database::defaultDataDir()));
}

} // namespace DentalPatients
