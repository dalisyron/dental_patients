#pragma once

#include "db/Patient.h"

#include <QAbstractTableModel>
#include <QVector>

namespace DentalPatients {

class PatientTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        Col_FileNumber = 0,
        Col_FullName,
        Col_Phone,
        Col_Notes,
        Col_Count
    };

    explicit PatientTableModel(QObject* parent = nullptr);

    void setPatients(QVector<Patient> patients);
    void clearAll();
    Patient patientAt(int row) const;
    int patientCount() const { return m_data.size(); }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QVector<Patient> m_data;
};

} // namespace DentalPatients
