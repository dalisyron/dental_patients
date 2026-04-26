#pragma once

#include <QString>

namespace DentalPatients::PersianText {

// Normalise Arabic-script variants to canonical Persian forms and strip
// joiners/diacritics so two visually identical names compare equal.
//
// Performs:
//   * Arabic Yeh (U+064A) -> Persian Yeh (U+06CC)
//   * Arabic Kaf (U+0643) -> Persian Keh (U+06A9)
//   * Arabic-Indic digits (U+0660-9) -> Persian (U+06F0-9)
//   * ASCII digits (0-9) -> Persian (U+06F0-9)
//   * Tatweel (U+0640) removed
//   * Harakat (U+064B-U+0652, U+0670) removed
//   * ZWNJ (U+200C), ZWJ (U+200D), BOM, RTL/LTR marks removed
//   * Multiple whitespace collapsed; leading/trailing trimmed
QString normalize(const QString& input);

// Same as normalize() but also lowercases ASCII (for file-number search).
QString normalizeForSearch(const QString& input);

// Convert ASCII digits to Persian digits for display ("123" -> "۱۲۳").
QString toPersianDigits(const QString& input);

// Convert Persian/Arabic-Indic digits to ASCII (for storage/search).
QString toAsciiDigits(const QString& input);

} // namespace DentalPatients::PersianText
