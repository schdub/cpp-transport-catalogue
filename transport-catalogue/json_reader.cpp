#include "json_reader.h"
#include "transport_catalogue.h"
#include "json.h"

#include <iostream>

using namespace json;

namespace tcatalogue {

std::optional<Document> JsonReader(std::istream & input) {
    std::optional<Document> doc;
    try {
        doc = Load(input);
    } catch(...) {
        // WARN() << "can't load json from stream";
    }
    return doc;
}

} // namespace tcatalogue
