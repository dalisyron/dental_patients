#include "db/CsvImporter.h"

#include "core/PersianText.h"
#include "db/PatientRepository.h"
#include "db/Patient.h"

#include <QFile>
#include <QtGlobal>
#include <QStringList>
#include <QTextStream>
#include <QVector>

#include <initializer_list>

namespace DentalPatients {

namespace {

// Minimal RFC 4180 CSV row parser - handles quoted fields and embedded commas.
// We don't use QString::split because field values may contain commas.
QStringList parseRow(const QString& line) {
    QStringList fields;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
                    current.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current.append(c);
            }
        } else {
            if (c == QLatin1Char(',')) {
                fields << current;
                current.clear();
            } else if (c == QLatin1Char('"')) {
                inQuotes = true;
            } else {
                current.append(c);
            }
        }
    }
    fields << current;
    return fields;
}

int columnIndex(const QStringList& header, std::initializer_list<QString> candidates) {
    for (int i = 0; i < header.size(); ++i) {
        QString h = header[i].trimmed().toLower();
        // Strip BOM if present on the very first column.
        if (!h.isEmpty() && h.front().unicode() == 0xFEFF) h.remove(0, 1);
        for (const auto& c : candidates) {
            if (h == c.toLower()) return i;
        }
    }
    return -1;
}

} // namespace

CsvImporter::Result CsvImporter::importFromFile(const QString& path, PatientRepository& repo) {
    Result r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        r.error = QStringLiteral("could not open CSV: %1 (%2)").arg(path, f.errorString());
        return r;
    }

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);

    QString headerLine;
    if (!ts.readLineInto(&headerLine)) {
        r.error = QStringLiteral("CSV is empty");
        return r;
    }
    if (!headerLine.isEmpty() && headerLine.front().unicode() == 0xFEFF) {
        headerLine.remove(0, 1);
    }
    const QStringList header = parseRow(headerLine);

    const int nameIdx = columnIndex(header, {QStringLiteral("patient name"), QStringLiteral("name"), QStringLiteral("full name")});
    const int fileIdx = columnIndex(header, {QStringLiteral("case number"), QStringLiteral("file number"), QStringLiteral("file no"), QStringLiteral("case no")});
    if (nameIdx < 0 || fileIdx < 0) {
        r.error = QStringLiteral("CSV header missing required columns (Patient Name, Case Number). Got: %1")
                      .arg(header.join(QStringLiteral(", ")));
        return r;
    }

    QVector<Patient> batch;
    batch.reserve(8192);

    QString line;
    while (ts.readLineInto(&line)) {
        if (line.trimmed().isEmpty()) {
            ++r.skipped;
            continue;
        }
        const QStringList row = parseRow(line);
        if (row.size() <= qMax(nameIdx, fileIdx)) {
            ++r.skipped;
            continue;
        }

        Patient p;
        const auto parts = PersianText::splitFamilyGiven(row.at(nameIdx).trimmed());
        p.familyName = parts.familyName;
        p.givenName  = parts.givenName;
        p.fileNumber = row.at(fileIdx).trimmed();
        if ((p.familyName.isEmpty() && p.givenName.isEmpty()) || p.fileNumber.isEmpty()) {
            ++r.skipped;
            continue;
        }

        batch.append(p);
        ++r.parsed;
    }

    QString err;
    const int imported = repo.insertMany(batch, &err);
    if (!err.isEmpty()) {
        r.error = err;
        return r;
    }
    r.imported = imported;
    r.ok       = true;
    return r;
}

} // namespace DentalPatients
