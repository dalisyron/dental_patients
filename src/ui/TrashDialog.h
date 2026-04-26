#pragma once

#include "db/Patient.h"

#include <QDialog>
#include <QVector>

class QTableWidget;

namespace DentalPatients {

class PatientRepository;

class TrashDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrashDialog(PatientRepository* repo, QWidget* parent = nullptr);

signals:
    void restored();

private slots:
    void refresh();
    void onRestore();

private:
    void buildUi();
    QVector<Patient> m_data;
    PatientRepository* m_repo;
    QTableWidget* m_table = nullptr;
};

} // namespace DentalPatients
