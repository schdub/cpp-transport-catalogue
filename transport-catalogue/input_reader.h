#pragma once

#include <iosfwd>

namespace tcatalogue {

class TransportCatalogue;
void InputReader(std::istream & input, TransportCatalogue & db);

} // namespace tcatalogue
