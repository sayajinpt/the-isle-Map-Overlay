#include "core/CoordinateParser.h"

#include <QRegularExpression>

namespace isle {
namespace {

double toFloatComponent(QString token)
{
    token.remove(QLatin1Char(','));
    bool ok = false;
    const double value = token.toDouble(&ok);
    return ok ? value : qQNaN();
}

} // namespace

std::optional<ParsedCoordinates> parseCoordinates(const QString &text)
{
    if (text.isEmpty() || text.size() > 256 || text.contains(QLatin1Char('\n'))
        || text.contains(QLatin1Char('\r'))) {
        return std::nullopt;
    }

    static const QRegularExpression number(
        QStringLiteral(R"([+-]?(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?)"));
    static const QRegularExpression plain(
        QStringLiteral(
            R"(^\s*(?<x>[+-]?(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?)\s*,\s*(?<y>[+-]?(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?)\s*,\s*(?<z>[+-]?(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?)\s*$)"));
    static const QRegularExpression labeled(
        QStringLiteral(
            R"(^\s*(?:X\s*[:=]\s*)?(?<x>[+-]?(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?)\s*[,;]\s*(?:Y\s*[:=]\s*)?(?<y>[+-]?(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?)\s*[,;]\s*(?:Z\s*[:=]\s*)?(?<z>[+-]?(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?)\s*$)"),
        QRegularExpression::CaseInsensitiveOption);

    Q_UNUSED(number);
    auto match = plain.match(text);
    if (!match.hasMatch()) {
        match = labeled.match(text);
    }
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    ParsedCoordinates values;
    values.x = toFloatComponent(match.captured(QStringLiteral("x")));
    values.y = toFloatComponent(match.captured(QStringLiteral("y")));
    values.z = toFloatComponent(match.captured(QStringLiteral("z")));
    if (!qIsFinite(values.x) || !qIsFinite(values.y) || !qIsFinite(values.z)) {
        return std::nullopt;
    }
    if (qAbs(values.x) > 10'000'000 || qAbs(values.y) > 10'000'000
        || qAbs(values.z) > 10'000'000) {
        return std::nullopt;
    }
    return values;
}

std::optional<ParsedCoordinates> parseCoordinatesLoose(const QString &text)
{
    if (const auto strict = parseCoordinates(text)) {
        return strict;
    }
    // OCR / HUD text: Lat/Long/Alt anywhere in a block.
    static const QRegularExpression labeled(
        QStringLiteral(
            R"((?:lat|x)\s*[:=]?\s*([+-]?\d[\d\s.,]*)[^\d+-]+(?:long|lng|y)\s*[:=]?\s*([+-]?\d[\d\s.,]*)(?:[^\d+-]+(?:alt|z)\s*[:=]?\s*([+-]?\d[\d\s.,]*))?)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = labeled.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    auto toNumber = [](QString token) -> double {
        token.remove(QLatin1Char(' '));
        if (token.contains(QLatin1Char(',')) && token.contains(QLatin1Char('.'))) {
            if (token.lastIndexOf(QLatin1Char(',')) > token.lastIndexOf(QLatin1Char('.'))) {
                token.remove(QLatin1Char('.'));
                token.replace(QLatin1Char(','), QLatin1Char('.'));
            } else {
                token.remove(QLatin1Char(','));
            }
        } else if (token.contains(QLatin1Char(','))) {
            const auto parts = token.split(QLatin1Char(','));
            if (parts.size() == 2 && parts[1].size() <= 3) {
                token = parts[0] + QLatin1Char('.') + parts[1];
            } else {
                token.remove(QLatin1Char(','));
            }
        }
        bool ok = false;
        const double value = token.toDouble(&ok);
        return ok ? value : qQNaN();
    };
    ParsedCoordinates values;
    values.x = toNumber(match.captured(1));
    values.y = toNumber(match.captured(2));
    values.z = match.lastCapturedIndex() >= 3 ? toNumber(match.captured(3)) : 0.0;
    if (!qIsFinite(values.x) || !qIsFinite(values.y)) {
        return std::nullopt;
    }
    if (!qIsFinite(values.z)) {
        values.z = 0.0;
    }
    return values;
}

} // namespace isle
