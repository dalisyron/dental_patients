#include "core/PersianText.h"

#include <QChar>
#include <QStringList>

namespace DentalPatients::PersianText {

namespace {

inline bool isHarakat(QChar c) {
    const ushort u = c.unicode();
    return (u >= 0x064B && u <= 0x0652) || u == 0x0670;
}

inline bool isInvisibleFormat(QChar c) {
    const ushort u = c.unicode();
    return u == 0x200C || u == 0x200D ||              // ZWNJ, ZWJ
           u == 0x200E || u == 0x200F ||              // LRM, RLM
           u == 0xFEFF ||                              // BOM
           u == 0x202A || u == 0x202B || u == 0x202C || // LRE, RLE, PDF
           u == 0x202D || u == 0x202E ||              // LRO, RLO
           u == 0x2066 || u == 0x2067 || u == 0x2068 || u == 0x2069;
}

QString core(const QString& input, bool lowercaseAscii) {
    QString out;
    out.reserve(input.size());
    bool prevSpace = true; // collapse leading whitespace
    for (QChar c : input) {
        const ushort u = c.unicode();

        if (isHarakat(c) || isInvisibleFormat(c) || u == 0x0640 /* tatweel */) {
            continue;
        }

        // Variant unification.
        if (u == 0x064A)      c = QChar(0x06CC);                // Arabic Yeh
        else if (u == 0x0649) c = QChar(0x06CC);                // Alef Maksura
        else if (u == 0x0643) c = QChar(0x06A9);                // Arabic Kaf
        else if (u >= 0x0660 && u <= 0x0669) c = QChar(u - 0x0660 + 0x06F0); // AR-Indic digits
        else if (u >= '0' && u <= '9')        c = QChar(u - '0'  + 0x06F0); // ASCII digits

        if (lowercaseAscii && u >= 'A' && u <= 'Z') c = QChar(u + 32);

        if (c.isSpace()) {
            if (!prevSpace) out.append(QLatin1Char(' '));
            prevSpace = true;
        } else {
            out.append(c);
            prevSpace = false;
        }
    }
    while (!out.isEmpty() && out.back() == QLatin1Char(' ')) out.chop(1);
    return out;
}

} // namespace

QString normalize(const QString& input) {
    return core(input, /*lowercaseAscii=*/false);
}

QString normalizeForSearch(const QString& input) {
    return core(input, /*lowercaseAscii=*/true);
}

NameParts splitFamilyGiven(const QString& fullName) {
    const QString normalised = normalize(fullName);
    const QStringList parts = normalised.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return {};
    if (parts.size() == 1) return {{}, parts.first()};

    NameParts out;
    out.givenName = parts.last();
    out.familyName = parts.mid(0, parts.size() - 1).join(QLatin1Char(' '));
    return out;
}

QString displayName(const QString& givenName, const QString& familyName) {
    return normalize(givenName + QLatin1Char(' ') + familyName);
}

QString toPersianDigits(const QString& input) {
    QString out = input;
    for (QChar& c : out) {
        const ushort u = c.unicode();
        if (u >= '0' && u <= '9') c = QChar(u - '0' + 0x06F0);
        else if (u >= 0x0660 && u <= 0x0669) c = QChar(u - 0x0660 + 0x06F0);
    }
    return out;
}

QString toAsciiDigits(const QString& input) {
    QString out = input;
    for (QChar& c : out) {
        const ushort u = c.unicode();
        if (u >= 0x06F0 && u <= 0x06F9) c = QChar(u - 0x06F0 + '0');
        else if (u >= 0x0660 && u <= 0x0669) c = QChar(u - 0x0660 + '0');
    }
    return out;
}

} // namespace DentalPatients::PersianText
