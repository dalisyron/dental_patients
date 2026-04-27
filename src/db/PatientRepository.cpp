#include "db/PatientRepository.h"

#include "core/PersianText.h"

#include <QDateTime>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <utility>

namespace DentalPatients {

namespace {

qint64 nowSeconds() { return QDateTime::currentSecsSinceEpoch(); }

QString fileNumberForStorage(const QString& value) {
    return PersianText::toAsciiDigits(PersianText::normalize(value));
}

QString phoneForStorage(const QString& value) {
    return PersianText::toAsciiDigits(value.isNull() ? QStringLiteral("") : value.trimmed());
}

QString textForStorage(const QString& value) {
    return value.isNull() ? QStringLiteral("") : value;
}

QString nameForStorage(const QString& value) {
    const QString normalised = PersianText::normalize(value);
    return normalised.isNull() ? QStringLiteral("") : normalised;
}

QString searchTextFor(const Patient& p, const QString& fileNumber, const QString& phone, const QString& notes) {
    const QString display = p.displayName();
    return PersianText::normalizeForSearch(p.familyName + QLatin1Char(' ')
                                          + p.givenName + QLatin1Char(' ')
                                          + display + QLatin1Char(' ')
                                          + fileNumber + QLatin1Char(' ')
                                          + phone + QLatin1Char(' ')
                                          + notes);
}

// FTS5 prefix-match query: "ali ahm" -> "ali* ahm*" so partial typing matches.
QString toFtsPrefixQuery(const QString& normalised) {
    const QStringList parts = normalised.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList terms;
    terms.reserve(parts.size());
    for (auto p : parts) {
        // Escape FTS5 special characters by quoting each term.
        p.replace(QLatin1Char('"'), QLatin1String("\"\""));
        terms << QStringLiteral("\"%1\"*").arg(p);
    }
    return terms.join(QLatin1Char(' '));
}

Patient readPatient(const QSqlQuery& q) {
    Patient p;
    p.id         = q.value(0).toLongLong();
    p.fileNumber = q.value(1).toString();
    p.familyName = q.value(2).toString();
    p.givenName  = q.value(3).toString();
    p.phone      = q.value(4).toString();
    p.notes      = q.value(5).toString();
    p.createdAt  = q.value(6).toLongLong();
    p.updatedAt  = q.value(7).toLongLong();
    return p;
}

constexpr auto kSelectColumns =
    "SELECT id, file_number, family_name, given_name, phone, notes, created_at, updated_at FROM patients";

QString orderBy(PatientRepository::SortField sortField, bool ascending) {
    const QString dir = ascending ? QStringLiteral("ASC") : QStringLiteral("DESC");
    if (sortField == PatientRepository::SortField::FileNumber) {
        return QStringLiteral(
            " ORDER BY CAST(file_number AS INTEGER) %1, file_number COLLATE NOCASE %1, "
            "family_name COLLATE NOCASE ASC, given_name COLLATE NOCASE ASC").arg(dir);
    }
    return QStringLiteral(
        " ORDER BY family_name COLLATE NOCASE %1, given_name COLLATE NOCASE %1, "
        "CAST(file_number AS INTEGER) ASC, file_number COLLATE NOCASE ASC").arg(dir);
}

} // namespace

PatientRepository::PatientRepository(QSqlDatabase db) : m_db(std::move(db)) {}

std::optional<qint64> PatientRepository::insert(const Patient& p, QString* error) {
    const qint64 ts = p.createdAt > 0 ? p.createdAt : nowSeconds();
    const QString fileNumber = fileNumberForStorage(p.fileNumber);
    Patient stored = p;
    stored.familyName = nameForStorage(p.familyName);
    stored.givenName = nameForStorage(p.givenName);
    const QString phone = phoneForStorage(p.phone);
    const QString notes = textForStorage(p.notes);
    const QString display = stored.displayName();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO patients(file_number, full_name, family_name, given_name, search_text, phone, notes, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);"));
    q.addBindValue(fileNumber);
    q.addBindValue(display);
    q.addBindValue(stored.familyName);
    q.addBindValue(stored.givenName);
    q.addBindValue(searchTextFor(stored, fileNumber, phone, notes));
    q.addBindValue(phone);
    q.addBindValue(notes);
    q.addBindValue(ts);
    q.addBindValue(p.updatedAt > 0 ? p.updatedAt : ts);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return std::nullopt;
    }
    return q.lastInsertId().toLongLong();
}

int PatientRepository::insertMany(const QVector<Patient>& ps, QString* error) {
    int imported = 0;
    if (!m_db.transaction()) {
        if (error) *error = m_db.lastError().text();
        return 0;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO patients(file_number, full_name, family_name, given_name, search_text, phone, notes, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);"));

    for (const Patient& p : ps) {
        const qint64 ts = p.createdAt > 0 ? p.createdAt : nowSeconds();
        const QString fileNumber = fileNumberForStorage(p.fileNumber);
        Patient stored = p;
        stored.familyName = nameForStorage(p.familyName);
        stored.givenName = nameForStorage(p.givenName);
        const QString phone = phoneForStorage(p.phone);
        const QString notes = textForStorage(p.notes);
        q.bindValue(0, fileNumber);
        q.bindValue(1, stored.displayName());
        q.bindValue(2, stored.familyName);
        q.bindValue(3, stored.givenName);
        q.bindValue(4, searchTextFor(stored, fileNumber, phone, notes));
        q.bindValue(5, phone);
        q.bindValue(6, notes);
        q.bindValue(7, ts);
        q.bindValue(8, p.updatedAt > 0 ? p.updatedAt : ts);
        if (!q.exec()) {
            if (error) *error = q.lastError().text();
            m_db.rollback();
            return 0;
        }
        ++imported;
    }

    if (!m_db.commit()) {
        if (error) *error = m_db.lastError().text();
        m_db.rollback();
        return 0;
    }
    return imported;
}

bool PatientRepository::update(const Patient& p, QString* error) {
    const QString fileNumber = fileNumberForStorage(p.fileNumber);
    Patient stored = p;
    stored.familyName = nameForStorage(p.familyName);
    stored.givenName = nameForStorage(p.givenName);
    const QString phone = phoneForStorage(p.phone);
    const QString notes = textForStorage(p.notes);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE patients "
        "SET file_number = ?, full_name = ?, family_name = ?, given_name = ?, search_text = ?, phone = ?, notes = ?, updated_at = ? "
        "WHERE id = ?;"));
    q.addBindValue(fileNumber);
    q.addBindValue(stored.displayName());
    q.addBindValue(stored.familyName);
    q.addBindValue(stored.givenName);
    q.addBindValue(searchTextFor(stored, fileNumber, phone, notes));
    q.addBindValue(phone);
    q.addBindValue(notes);
    q.addBindValue(nowSeconds());
    q.addBindValue(p.id);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() == 1;
}

bool PatientRepository::softDelete(qint64 id, QString* error) {
    if (!m_db.transaction()) {
        if (error) *error = m_db.lastError().text();
        return false;
    }
    QSqlQuery copy(m_db);
    copy.prepare(QStringLiteral(
        "INSERT INTO patients_trash(id, file_number, full_name, family_name, given_name, search_text, phone, notes, created_at, updated_at, deleted_at) "
        "SELECT id, file_number, full_name, family_name, given_name, search_text, phone, notes, created_at, updated_at, ? FROM patients WHERE id = ?;"));
    copy.addBindValue(nowSeconds());
    copy.addBindValue(id);
    if (!copy.exec()) {
        if (error) *error = copy.lastError().text();
        m_db.rollback();
        return false;
    }
    if (copy.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }
    QSqlQuery del(m_db);
    del.prepare(QStringLiteral("DELETE FROM patients WHERE id = ?;"));
    del.addBindValue(id);
    if (!del.exec()) {
        if (error) *error = del.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool PatientRepository::restoreFromTrash(qint64 id, QString* error) {
    if (!m_db.transaction()) {
        if (error) *error = m_db.lastError().text();
        return false;
    }
    QSqlQuery restore(m_db);
    restore.prepare(QStringLiteral(
        "INSERT INTO patients(id, file_number, full_name, family_name, given_name, search_text, phone, notes, created_at, updated_at) "
        "SELECT id, file_number, full_name, family_name, given_name, search_text, phone, notes, created_at, updated_at FROM patients_trash WHERE id = ?;"));
    restore.addBindValue(id);
    if (!restore.exec()) {
        if (error) *error = restore.lastError().text();
        m_db.rollback();
        return false;
    }
    if (restore.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }
    QSqlQuery del(m_db);
    del.prepare(QStringLiteral("DELETE FROM patients_trash WHERE id = ?;"));
    del.addBindValue(id);
    if (!del.exec()) {
        if (error) *error = del.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool PatientRepository::purgeTrashOlderThan(int days, QString* error) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM patients_trash WHERE deleted_at < ?;"));
    q.addBindValue(nowSeconds() - qint64(days) * 86400);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    return true;
}

int PatientRepository::count() const {
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM patients;")) && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

int PatientRepository::trashCount() const {
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM patients_trash;")) && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

std::optional<Patient> PatientRepository::findById(qint64 id) const {
    QSqlQuery q(m_db);
    q.prepare(QString::fromLatin1(kSelectColumns) + QStringLiteral(" WHERE id = ?;"));
    q.addBindValue(id);
    if (q.exec() && q.next()) return readPatient(q);
    return std::nullopt;
}

std::optional<Patient> PatientRepository::findByFileNumber(const QString& fileNumber) const {
    QSqlQuery q(m_db);
    q.prepare(QString::fromLatin1(kSelectColumns) + QStringLiteral(
        " WHERE file_number = ? ORDER BY family_name COLLATE NOCASE, given_name COLLATE NOCASE LIMIT 1;"));
    q.addBindValue(fileNumberForStorage(fileNumber));
    if (q.exec() && q.next()) return readPatient(q);
    return std::nullopt;
}

std::optional<QString> PatientRepository::nextAvailableFileNumber(qint64 startingAt, QString* error) const {
    if (startingAt < 0) startingAt = 0;

    QSet<qint64> usedNumbers;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT file_number FROM patients "
            "UNION ALL "
            "SELECT file_number FROM patients_trash;"))) {
        if (error) *error = q.lastError().text();
        return std::nullopt;
    }

    while (q.next()) {
        const QString fileNumber = fileNumberForStorage(q.value(0).toString());
        bool ok = false;
        const qint64 number = fileNumber.toLongLong(&ok);
        if (ok && number >= startingAt && QString::number(number) == fileNumber) {
            usedNumbers.insert(number);
        }
    }

    qint64 candidate = startingAt;
    while (usedNumbers.contains(candidate)) {
        ++candidate;
    }
    return QString::number(candidate);
}

QVector<Patient> PatientRepository::search(const QString& query,
                                           int limit,
                                           SortField sortField,
                                           bool ascending) const {
    QVector<Patient> results;
    const QString normalised = PersianText::normalizeForSearch(query);
    const QString sortClause = orderBy(sortField, ascending);

    QSqlQuery q(m_db);
    if (normalised.isEmpty()) {
        q.prepare(QString::fromLatin1(kSelectColumns) + sortClause + QStringLiteral(" LIMIT ?;"));
        q.addBindValue(limit);
    } else {
        q.prepare(QString::fromLatin1(kSelectColumns) + QStringLiteral(
            " WHERE id IN (SELECT rowid FROM patients_fts WHERE patients_fts MATCH ?) "
        ) + sortClause + QStringLiteral(" LIMIT ?;"));
        q.addBindValue(toFtsPrefixQuery(normalised));
        q.addBindValue(limit);
    }
    if (!q.exec()) return {};
    while (q.next()) results.append(readPatient(q));
    return results;
}

QVector<Patient> PatientRepository::trash(int limit) const {
    QVector<Patient> results;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, file_number, family_name, given_name, phone, notes, created_at, updated_at "
        "FROM patients_trash ORDER BY deleted_at DESC LIMIT ?;"));
    q.addBindValue(limit);
    if (!q.exec()) return {};
    while (q.next()) results.append(readPatient(q));
    return results;
}

QString PatientRepository::meta(const QString& key) const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?;"));
    q.addBindValue(key);
    if (q.exec() && q.next()) return q.value(0).toString();
    return {};
}

bool PatientRepository::setMeta(const QString& key, const QString& value) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO meta(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;"));
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

bool PatientRepository::isInitialized() const {
    return count() > 0
        || !meta(QStringLiteral("app_initialized")).isEmpty()
        || !meta(QStringLiteral("csv_import_done")).isEmpty();
}

bool PatientRepository::markInitialized() {
    return setMeta(QStringLiteral("app_initialized"),
                   QString::number(QDateTime::currentSecsSinceEpoch()));
}

void PatientRepository::resetDatabase(QSqlDatabase db) {
    m_db = std::move(db);
}

} // namespace DentalPatients
