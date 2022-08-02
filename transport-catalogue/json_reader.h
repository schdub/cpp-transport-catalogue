#pragma once

#include <iosfwd>
#include <optional>

#include "json.h"

namespace tcatalogue {

std::optional<json::Document> JsonReader(std::istream & input);

} // namespace tcatalogue
