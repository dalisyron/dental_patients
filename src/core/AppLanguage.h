#pragma once

#include <QString>

class QApplication;

namespace DentalPatients::AppLanguage {

enum class Language { English, Persian };

// The persisted UI language (QSettings "ui/language"). Defaults to English.
Language current();
void setCurrent(Language lang);

// Cached per process: the language cannot change without a restart, and this
// is queried from per-cell display code.
bool isPersian();

// "Dental Patients" or its Persian product name, per the active language.
QString appDisplayName();

// Persian-digit rendering in Persian mode; the input unchanged otherwise.
// Storage always keeps ASCII digits — this is display-only.
QString localizeDigits(const QString& input);

// Locale, layout direction, application display name, and (for Persian) the
// translator. Must run once at startup, before any widget is created.
void applyToApplication(QApplication& app);

// Tests only: force the process-wide language without touching stored
// settings (isPersian() is otherwise cached for the process lifetime).
void overrideForTesting(Language lang);

} // namespace DentalPatients::AppLanguage
