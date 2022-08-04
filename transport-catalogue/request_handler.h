#pragma once

#include "domain.h"
#include "transport_catalogue.h"
#include "map_renderer.h"

class RequestHandler {
    tcatalogue::TransportCatalogue & db_;
    renderer::MapRenderer & drawer_;

public:
    RequestHandler(tcatalogue::TransportCatalogue & db,
                   renderer::MapRenderer & drawer)
    : db_(db)
    , drawer_(drawer)
    {}

    const domain::Bus& GetBus(domain::BusId id) const;

    domain::StopBusesOpt GetStopBuses(std::string_view stop_name) const;

    std::pair<double, double> CalculateRouteLength(const domain::Bus & bus) const;

    std::vector<const domain::Bus*> GetAllBuses() const;

    std::string DrawMap() const;
};
