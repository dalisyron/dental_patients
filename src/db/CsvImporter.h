#pragma once

#include <QString>

namespace DentalPatients {

class PatientRepository;

class CsvImporter {
public:
    struct Result {
        bool    ok        = false;
        int     parsed    = 0;
        int     imported  = 0;
        int     skipped   = 0;     // blank or malformed rows skipped during import
        QString error;
    };

    // Imports the BOM-prefixed UTF-8 CSV at `path` into `repo`.
    // The CSV must have a header row with at least "Patient Name" and "Case Number"
    // / "File Number" columns (in either order).
    static Result importFromFile(const QString& path, PatientRepository& repo);
};

} // namespace DentalPatients
