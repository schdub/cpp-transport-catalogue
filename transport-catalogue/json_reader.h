#pragma once

#include <iosfwd>
#include <optional>
#include "map_renderer.h"
#include "domain.h"

#include "json.h"

namespace tcatalogue {

class JsonReader {
    std::optional<json::Document> doc_;

public:
    explicit JsonReader(std::istream & input);

    bool IsOk() const;
    void ParseInput(domain::STOPS & stops, domain::BUSES &buses);
    void ParseStatRequests(domain::STAT_REQUESTS & requests);
    std::optional<renderer::Settings> ParseRenderSettings();
};

} // namespace tcatalogue
