#include "ui/MainWindow.h"

#include "Version.h"
#include "core/AppLanguage.h"
#include "core/PersianText.h"
#include "db/Database.h"
#include "db/PatientRepository.h"
#include "ui/AboutDialog.h"
#include "ui/PatientDialog.h"
#include "ui/PatientTableModel.h"
#include "ui/TrashDialog.h"

#include <QAction>
#include <QActionGroup>
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
#include <QProcess>
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
constexpr int kFirstRunBodyLines = 3;
constexpr int kFirstRunBodyVerticalPadding = 12;

QString backupFileFilter() {
    return MainWindow::tr("Dental Patients backup files (*.dpbackup)");
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

void showMessage(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text) {
    QMessageBox box(parent);
    box.setIcon(icon);
    box.setWindowTitle(title);
    box.setText(text);
    auto* okButton = box.addButton(MainWindow::tr("OK"), QMessageBox::AcceptRole);
    box.setDefaultButton(okButton);
    box.setEscapeButton(okButton);
    box.exec();
}

void showInformation(QWidget* parent, const QString& title, const QString& text) {
    showMessage(parent, QMessageBox::Information, title, text);
}

void showWarning(QWidget* parent, const QString& title, const QString& text) {
    showMessage(parent, QMessageBox::Warning, title, text);
}

void showCritical(QWidget* parent, const QString& title, const QString& text) {
    showMessage(parent, QMessageBox::Critical, title, text);
}

} // namespace

MainWindow::MainWindow(PatientRepository* repo, QWidget* parent)
    : QMainWindow(parent), m_repo(repo) {
    setWindowTitle(QStringLiteral("%1  -  v%2")
                       .arg(AppLanguage::appDisplayName(),
                            QString::fromUtf8(Version::kString)));
    resize(1100, 720);

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
    m_search->setPlaceholderText(tr("Search name, case number, phone..."));
    m_search->setClearButtonEnabled(true);
    if (AppLanguage::isPersian()) {
        m_search->setLayoutDirection(Qt::RightToLeft);
        m_search->setAlignment(Qt::AlignRight | Qt::AlignAbsolute | Qt::AlignVCenter);
    }
    m_search->setCursorMoveStyle(Qt::LogicalMoveStyle);
    m_search->setMinimumHeight(36);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);

    m_addButton = new QPushButton(tr("Add Patient"));
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
    auto* findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(findShortcut, &QShortcut::activated, this, [this]{ m_search->setFocus(); m_search->selectAll(); });
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

    auto* title = new QLabel(tr("Initial patient data setup"));
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* body = new QLabel(tr("To get started, load a Dental Patients backup file (.dpbackup) or start with an empty database."));
    body->setAlignment(Qt::AlignCenter);
    body->setWordWrap(true);
    body->setMaximumWidth(kFirstRunTextMaxWidth);
    body->setMinimumHeight(body->fontMetrics().lineSpacing() * kFirstRunBodyLines
                           + kFirstRunBodyVerticalPadding);

    auto* loadBackup = new QPushButton(tr("Load backup file (.dpbackup)"));
    loadBackup->setObjectName(QStringLiteral("primaryButton"));
    loadBackup->setMinimumHeight(38);
    loadBackup->setMinimumWidth(260);
    connect(loadBackup, &QPushButton::clicked, this, &MainWindow::onLoadInitialBackup);

    auto* startEmpty = new QPushButton(tr("Start with an empty database"));
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
    auto* file = menuBar()->addMenu(tr("&File"));

    m_actAdd = file->addAction(tr("Add new patient..."), QKeySequence::New, this, &MainWindow::onAddClicked);
    m_actAdd->setShortcutContext(Qt::WindowShortcut);
    addAction(m_actAdd);
    m_actEdit = file->addAction(tr("Edit selected patient..."), this, &MainWindow::onEditCurrent);
    m_actEdit->setShortcut(QKeySequence(Qt::Key_F2));
    m_actEdit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actDelete = file->addAction(tr("Delete selected patient..."), this, &MainWindow::onDeleteCurrent);
    m_actDelete->setShortcut(QKeySequence(Qt::Key_Delete));
    m_actDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    if (m_table) {
        m_table->addAction(m_actEdit);
        m_table->addAction(m_actDelete);
    }
    updatePatientActions();
    file->addSeparator();
    m_actExport = file->addAction(tr("Export CSV..."), this, &MainWindow::onExportCsv);
    file->addSeparator();
    m_actQuit = file->addAction(tr("Quit"), QKeySequence::Quit, this, &QWidget::close);

    auto* tools = menuBar()->addMenu(tr("&Tools"));
    m_actBackup = tools->addAction(tr("Create backup"), this, &MainWindow::onBackupNow);
    m_actRestore = tools->addAction(tr("Restore from backup..."), this, &MainWindow::onRestoreBackup);
    m_actTrash  = tools->addAction(tr("Recycle bin..."), this, &MainWindow::onShowTrash);
    tools->addSeparator();
    m_actDataLoc = tools->addAction(tr("Show data folder"), this, &MainWindow::onShowDataLocation);

    // Language names are deliberately shown in their own script, untranslated.
    auto* language = menuBar()->addMenu(tr("&Language"));
    auto* languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    auto addLanguage = [this, language, languageGroup](const QString& name,
                                                       AppLanguage::Language lang) {
        QAction* action = language->addAction(name);
        action->setCheckable(true);
        action->setChecked(AppLanguage::current() == lang);
        languageGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, lang] { switchLanguage(lang); });
    };
    addLanguage(QStringLiteral("English"), AppLanguage::Language::English);
    addLanguage(QStringLiteral("فارسی"), AppLanguage::Language::Persian);

    auto* help = menuBar()->addMenu(tr("&Help"));
    m_actAbout = help->addAction(tr("About..."), this, &MainWindow::onAbout);
}

void MainWindow::switchLanguage(AppLanguage::Language lang) {
    if (AppLanguage::current() == lang) return;
    AppLanguage::setCurrent(lang);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Language changed"));
    box.setText(tr("Restart the application now to apply the new language?"));
    auto* restartButton = box.addButton(tr("Restart now"), QMessageBox::AcceptRole);
    auto* laterButton = box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.setDefaultButton(restartButton);
    box.setEscapeButton(laterButton);
    box.exec();

    if (box.clickedButton() == restartButton) {
        // The fresh process waits (--relaunched) for this one to release the
        // single-instance server and the database before starting up.
        QProcess::startDetached(QApplication::applicationFilePath(),
                                {QStringLiteral("--relaunched")});
        close();
    }
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
        m_statusLabel->setText(tr("Ready for initial setup"));
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
    const QString msg = tr("Showing %1 of %2 patients")
                            .arg(AppLanguage::localizeDigits(QString::number(shown)),
                                 AppLanguage::localizeDigits(QString::number(total)));
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
    confirm.setWindowTitle(tr("Delete patient"));
    confirm.setText(tr("Delete “%1” (case number %2)?\nDeleted patients can be restored from the recycle bin.")
                        .arg(p.displayName(), AppLanguage::localizeDigits(p.fileNumber)));
    auto* yesButton = confirm.addButton(tr("Yes"), QMessageBox::YesRole);
    auto* noButton = confirm.addButton(tr("No"), QMessageBox::NoRole);
    confirm.setDefaultButton(noButton);
    confirm.setEscapeButton(noButton);
    confirm.exec();

    if (confirm.clickedButton() != yesButton) return;

    QString err;
    if (!m_repo->softDelete(p.id, &err)) {
        showCritical(this, tr("Error"),
            tr("Delete failed:\n%1").arg(err));
        return;
    }
    refreshTable(m_search->text());
    updatePatientActions();
}

void MainWindow::onExportCsv() {
    const QString suggested = QStringLiteral("patients-%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"),
                                                      suggested, tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showCritical(this, tr("Error"),
            tr("Could not open the file for writing:\n%1").arg(f.errorString()));
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
        showCritical(this, tr("Error"),
            tr("Export failed:\n%1").arg(detail));
        return;
    }
    f.close();
    if (f.error() != QFileDevice::NoError) {
        const QString detail = f.errorString();
        QFile::remove(path);
        showCritical(this, tr("Error"),
            tr("Export failed:\n%1").arg(detail));
        return;
    }
    showInformation(this, tr("Export complete"),
        tr("%1 patients were exported.").arg(AppLanguage::localizeDigits(QString::number(all.size()))));
}

void MainWindow::onBackupNow() {
    QString err;
    const QString path = Database::instance().createBackup(&err);
    if (path.isEmpty()) {
        showCritical(this, tr("Error"),
            tr("Backup failed:\n%1").arg(err));
        return;
    }
    Database::instance().rotateBackups(30);
    showInformation(this, tr("Backup"),
        tr("A new backup was created:\n%1").arg(path));
}

void MainWindow::onRestoreBackup() {
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Select a backup file"),
                                                      Database::backupDir(),
                                                      backupFileFilter());
    if (path.isEmpty()) return;
    restoreFromBackupFile(path, false);
}

void MainWindow::onLoadInitialBackup() {
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Select a backup file"),
                                                      QString(),
                                                      backupFileFilter());
    if (path.isEmpty()) return;
    restoreFromBackupFile(path, true);
}

void MainWindow::onStartEmptyDatabase() {
    if (!m_repo->markInitialized()) {
        showCritical(this, tr("Error"),
            tr("Initial setup could not be recorded."));
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
        showCritical(this, tr("Invalid backup file"),
            tr("This backup file is unreadable:\n%1").arg(inspectErr));
        return false;
    }

    const bool initial = initialLoad || isInitialSetupPending();
    const QString patientCount = AppLanguage::localizeDigits(QString::number(info.patientCount));
    QMessageBox confirm(this);
    confirm.setIcon(QMessageBox::Question);
    confirm.setWindowTitle(initial ? tr("Load initial data") : tr("Restore backup"));
    confirm.setText(initial
        ? tr("Patient data will be loaded from:\n%1\n\nPatients: %2")
              .arg(QFileInfo(path).fileName(), patientCount)
        : tr("Continuing will replace the data on this device with the contents of:\n%1\n\n"
             "Patients in the backup: %2\n\n"
             "A safety backup of the current data is created before restoring.")
              .arg(QFileInfo(path).fileName(), patientCount));
    auto* yesButton = confirm.addButton(initial ? tr("Load") : tr("Restore"), QMessageBox::AcceptRole);
    auto* noButton = confirm.addButton(tr("Cancel"), QMessageBox::RejectRole);
    confirm.setDefaultButton(noButton);
    confirm.setEscapeButton(noButton);
    confirm.exec();
    if (confirm.clickedButton() != yesButton) return false;

    if (!initial && m_repo->isInitialized()) {
        QString backupErr;
        const QString safetyBackup = Database::instance().createBackup(&backupErr);
        if (safetyBackup.isEmpty()) {
            showCritical(this, tr("Error"),
                tr("Before restoring, creating a safety backup failed:\n%1").arg(backupErr));
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
        showCritical(this, tr("Restore error"),
            tr("Restoring the backup failed:\n%1").arg(restoreErr));
        return false;
    }

    m_repo->resetDatabase(Database::instance().sql());
    if (!m_repo->markInitialized()) {
        showWarning(this, tr("Warning"),
            tr("The data was loaded, but the setup state could not be fully recorded."));
    }
    refreshTable();
    selectFirstRow();
    showInformation(this, tr("Restore complete"),
        initial ? tr("Initial data was loaded successfully.")
                : tr("The backup was restored successfully."));
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

void MainWindow::onSecondInstanceLaunched(const QStringList& args) {
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    show();
    raise();
    activateWindow();

    QString backupPath;
    for (int i = 1; i < args.size(); ++i) {
        const QFileInfo info(args.at(i));
        if (info.suffix().compare(QStringLiteral("dpbackup"), Qt::CaseInsensitive) == 0) {
            backupPath = info.absoluteFilePath();
            break;
        }
    }
    if (!backupPath.isEmpty()) {
        openBackupFile(backupPath);
    }
}

} // namespace DentalPatients
