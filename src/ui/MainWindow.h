#pragma once

#include "db/PatientRepository.h"

#include <QMainWindow>
#include <QTimer>

class QLineEdit;
class QTableView;
class QLabel;
class QAction;

namespace DentalPatients {

class PatientTableModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(PatientRepository* repo, QWidget* parent = nullptr);

private slots:
    void onSearchChanged(const QString&);
    void onHeaderClicked(int section);
    void onAddClicked();
    void onEditCurrent();
    void onDeleteCurrent();
    void onExportCsv();
    void onBackupNow();
    void onShowTrash();
    void onAbout();
    void onShowDataLocation();

private:
    void buildUi();
    void buildMenus();
    void refreshTable(const QString& query = {});
    PatientRepository::SortField currentSortField() const;
    void selectFirstRow();
    void updatePatientActions();
    void updateStatus(int total);
    void editCurrent(int initialFocusColumn);

    PatientRepository* m_repo;
    PatientTableModel* m_model = nullptr;

    QLineEdit* m_search = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_statusLabel = nullptr;

    QTimer m_searchDebounce;
    QString m_pendingQuery;
    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    QAction* m_actAdd = nullptr;
    QAction* m_actEdit = nullptr;
    QAction* m_actDelete = nullptr;
    QAction* m_actBackup = nullptr;
    QAction* m_actExport = nullptr;
    QAction* m_actTrash = nullptr;
    QAction* m_actAbout = nullptr;
    QAction* m_actDataLoc = nullptr;
    QAction* m_actQuit = nullptr;
};

} // namespace DentalPatients
