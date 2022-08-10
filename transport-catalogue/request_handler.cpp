#include "request_handler.h"
#include "transport_catalogue.h"
#include "domain.h"
#include "geo.h"
#include <cassert>
#include <sstream>

using namespace domain;

const domain::Bus& RequestHandler::GetBus(domain::BusId id) const {
    return db_.GetBus(id);
}

domain::StopBusesOpt RequestHandler::GetStopBuses(std::string_view stop_name) const {
    return db_.GetStopBuses(stop_name);
}

std::vector<const domain::Bus*> RequestHandler::GetAllBuses() const {
    std::vector<const domain::Bus*> result;
    for (const auto bus_id : db_) {
        result.push_back(db_.GetBusPtr(bus_id));
    }
    return result;
}

// расчитываем географическую и актуальную дистанцию для маршрута, указанного автобуса.
std::pair<double, double> RequestHandler::CalculateRouteLength(const domain::Bus & bus) const {
    double geographical = 0.0;
    double actual = 0.0;
    const auto & v = bus.stops;
    assert(v.size() >= 2);
    for (size_t i = 0; i + 1 < v.size(); ++i) {
        const Stop* pStopA = v[i];
        const Stop* pStopB = v[i+1];
        double distance = ComputeDistance( pStopA->coordinates, pStopB->coordinates );
        geographical += distance;
        size_t meters = db_.GetDistanceBetween( pStopA, pStopB );
        actual += meters;
    }
    if (bus.is_round_trip == false) {
        geographical *= 2;
        for (size_t i = v.size()-1; i > 0; --i) {
            const Stop* pStopA = v[i];
            const Stop* pStopB = v[i-1];
            size_t meters = db_.GetDistanceBetween( pStopA, pStopB );
            actual += meters;
        }
    }
    return std::make_pair(geographical, actual);
}

std::string RequestHandler::DrawMap() const {
    std::stringstream stream;
    svg::Document doc = drawer_.Render( GetAllBuses() );
    doc.Render(stream);
    return stream.str();
}
