#pragma once

#include "transport_catalogue.h"
#include "domain.h"

class RequestHandler {
    tcatalogue::TransportCatalogue & db_;

public:
    RequestHandler(tcatalogue::TransportCatalogue & db) : db_(db) {}

    std::vector<const domain::Bus*> GetAllBuses() const;
};
