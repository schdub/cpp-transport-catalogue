#include "input_reader.h"
#include "stat_reader.h"
#include "transport_catalogue.h"
#include <iostream>

using namespace tcatalogue;

int main() {
    TransportCatalogue db;
    InputReader(std::cin, db);
    StatReader(std::cin, std::cout, db);
}
