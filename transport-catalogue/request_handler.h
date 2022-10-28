#pragma once

#include "domain.h"
#include "transport_catalogue.h"
#include "map_renderer.h"
#include "graph.h"
#include "router.h"
#include <memory>
#include <limits>

class RouteGraph;

class RequestHandler {
    tcatalogue::TransportCatalogue & db_;
    renderer::MapRenderer & drawer_;

    mutable std::shared_ptr<RouteGraph> route_graph_;

public:
    RequestHandler(tcatalogue::TransportCatalogue & db,
                   renderer::MapRenderer & drawer);

    const domain::Bus& GetBus(domain::BusId id) const;

    domain::StopBusesOpt GetStopBuses(std::string_view stop_name) const;

    std::pair<double, double> CalculateRouteLength(const domain::Bus & bus) const;

    std::vector<const domain::Bus*> GetAllBuses() const;

    std::string DrawMap() const;

    bool HandleRoute(const domain::STAT_REQ_ROUTE & route_request,
                     domain::STAT_RESP_ROUTE & route_response) const;
};
