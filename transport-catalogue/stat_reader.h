#pragma once

#include <iosfwd>

namespace tcatalogue {

class TransportCatalogue;
void StatReader(std::istream & input, std::ostream & output, TransportCatalogue & db);

} // namespace tcatalogue
