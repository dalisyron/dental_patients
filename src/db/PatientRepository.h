#pragma once

#include "db/Patient.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace DentalPatients {

class PatientRepository {
public:
    explicit PatientRepository(QSqlDatabase db);

    // ---- mutations ---------------------------------------------------------
    std::optional<qint64> insert(const Patient& p, QString* error = nullptr);

    // Convenience for bulk loads (CSV import) - wraps work in one transaction.
    int insertMany(const QVector<Patient>& ps, int* skippedRows, QString* error);

    bool update(const Patient& p, QString* error = nullptr);
    bool softDelete(qint64 id, QString* error = nullptr);     // moves to patients_trash
    bool restoreFromTrash(qint64 id, QString* error = nullptr);
    bool purgeTrashOlderThan(int days, QString* error = nullptr);

    // ---- queries -----------------------------------------------------------
    int  count() const;
    int  trashCount() const;
    std::optional<Patient> findById(qint64 id) const;
    std::optional<Patient> findByFileNumber(const QString& fileNumber) const;

    // search() returns up to `limit` matches sorted by full_name. An empty
    // query returns the first `limit` patients alphabetically.
    QVector<Patient> search(const QString& query, int limit = 1000) const;
    QVector<Patient> trash(int limit = 1000) const;

    // ---- meta --------------------------------------------------------------
    QString meta(const QString& key) const;
    bool    setMeta(const QString& key, const QString& value);

private:
    QSqlDatabase m_db;
};

} // namespace DentalPatients
