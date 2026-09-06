#pragma once

#include "core/Position.h"

#include <QString>
#include <optional>

namespace isle {

struct ParsedCoordinates {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

std::optional<ParsedCoordinates> parseCoordinates(const QString &text);
std::optional<ParsedCoordinates> parseCoordinatesLoose(const QString &text);

} // namespace isle
