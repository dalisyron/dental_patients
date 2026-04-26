#pragma once

#include <QString>
#include <QDateTime>

namespace DentalPatients {

struct Patient {
    qint64  id          = -1;
    QString fileNumber;
    QString fullName;
    QString phone;
    QString notes;
    qint64  createdAt   = 0;   // unix seconds, UTC
    qint64  updatedAt   = 0;
};

inline bool operator==(const Patient& a, const Patient& b) noexcept {
    return a.id == b.id
        && a.fileNumber == b.fileNumber
        && a.fullName == b.fullName
        && a.phone == b.phone
        && a.notes == b.notes
        && a.createdAt == b.createdAt
        && a.updatedAt == b.updatedAt;
}

} // namespace DentalPatients
