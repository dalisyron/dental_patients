#include "ui/PatientTableModel.h"

#include "core/PersianText.h"

#include <utility>

namespace DentalPatients {

PatientTableModel::PatientTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void PatientTableModel::setPatients(QVector<Patient> patients) {
    beginResetModel();
    m_data = std::move(patients);
    endResetModel();
}

void PatientTableModel::clearAll() {
    beginResetModel();
    m_data.clear();
    endResetModel();
}

Patient PatientTableModel::patientAt(int row) const {
    if (row < 0 || row >= m_data.size()) return {};
    return m_data.at(row);
}

int PatientTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_data.size();
}

int PatientTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : Col_Count;
}

QVariant PatientTableModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= m_data.size()) return {};
    const Patient& p = m_data.at(idx.row());

    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
            case Col_FamilyName: return p.familyName;
            case Col_GivenName:  return p.givenName;
            case Col_FileNumber: return PersianText::toPersianDigits(p.fileNumber);
            case Col_Phone:      return PersianText::toPersianDigits(p.phone);
            case Col_Notes: {
                // Notes can be long - show single-line preview in the table.
                QString preview = p.notes;
                preview.replace(QLatin1Char('\n'), QLatin1Char(' '));
                if (preview.size() > 80) preview = preview.left(80) + QStringLiteral("…");
                return preview;
            }
            default: return {};
        }
    }
    if (role == Qt::TextAlignmentRole) {
        return int(Qt::AlignVCenter | Qt::AlignRight);
    }
    if (role == Qt::UserRole) {
        return QVariant::fromValue(p.id);
    }
    if (role == Qt::ToolTipRole && idx.column() == Col_Notes) {
        return p.notes;
    }
    return {};
}

QVariant PatientTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        switch (section) {
            case Col_FamilyName: return tr("نام خانوادگی");
            case Col_GivenName:  return tr("نام");
            case Col_FileNumber: return tr("شماره پرونده");
            case Col_Phone:      return tr("تلفن");
            case Col_Notes:      return tr("یادداشت");
            default: return {};
        }
    }
    return PersianText::toPersianDigits(QString::number(section + 1));
}

} // namespace DentalPatients
