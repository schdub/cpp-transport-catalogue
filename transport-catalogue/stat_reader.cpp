#include "stat_reader.h"
#include "transport_catalogue.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cassert>
#include <unordered_set>
#include <functional>
#include <algorithm>

namespace tcatalogue {

namespace detail {

std::pair<double, double> CalculateRouteLength(const Bus & bus, TransportCatalogue & db) {
    double geographical = 0.0;
    double actual = 0.0;
    const auto & v = bus.stops;
    assert(v.size() >= 2);
    for (size_t i = 0; i + 1 < v.size(); ++i) {
        const Stop* pStopA = v[i];
        const Stop* pStopB = v[i+1];
        double distance = ComputeDistance( pStopA->coordinates, pStopB->coordinates );
        geographical += distance;
        size_t meters = db.GetDistanceBetween( pStopA, pStopB );
        actual += meters;
    }
    if (bus.is_ring_root == false) {
        geographical *= 2;
        for (size_t i = v.size()-1; i > 0; --i) {
            const Stop* pStopA = v[i];
            const Stop* pStopB = v[i-1];
            size_t meters = db.GetDistanceBetween( pStopA, pStopB );
            actual += meters;
        }
    }
    return std::make_pair(geographical, actual);
}

size_t UniqueStopsCount(const Bus & bus) {
    std::unordered_set<Stop*> set(bus.stops.begin(), bus.stops.end());
    return set.size();
}

} // namespace detail

namespace parse {

bool Bus(std::string_view line, BusId & bus_id) {
    size_t idx = line.find_first_not_of(" \t");
    if (idx != std::string::npos) {
        bus_id = std::string{line.substr(idx)};
        return true;
    }
    return false;
}

bool Stop(std::string_view line, std::string & stop_name) {
    size_t idx = line.find_first_not_of(" \t");
    if (idx != std::string::npos) {
        stop_name = std::string{line.substr(idx)};
        return true;
    }
    return false;
}

} // namespace parse

namespace work {

void Bus(const BusId & bus_id, std::ostream & output, TransportCatalogue & db) {
    auto bus = db.GetBus(bus_id);
    output << "Bus " << bus_id << ": ";
    if (bus.stops.empty()) {
        output << "not found" << std::endl;
    } else {
        // 6 stops on route, 5 unique stops, 4371.02 route length
        size_t stops_on_route = (bus.is_ring_root
             ? bus.stops.size()
             : (bus.stops.size() * 2) - 1);
        size_t unique_stops = detail::UniqueStopsCount(bus);
        std::pair<double, double> route_length = detail::CalculateRouteLength(bus, db);
        double curvature = route_length.second / route_length.first;
        output << stops_on_route << " stops on route, "
               << unique_stops   << " unique stops, "
               << std::setprecision(6) << route_length.second << " route length, "
               << std::setprecision(6) << curvature << " curvature"
               << std::endl;
    }
}

void Stop(const std::string & stop_name, std::ostream & output, TransportCatalogue & db) {
    output << "Stop " << stop_name << ": ";
    auto opt_stop_busses = db.GetStopBuses(stop_name);
    if (opt_stop_busses.has_value() == false) {
        output << "not found" << std::endl;
    } else {
        const auto & stop_busses = opt_stop_busses.value().get();
        if (stop_busses.size() == 0) {
            output << "no buses" << std::endl;
        } else {
            std::vector<std::string_view> vector_stops;
            vector_stops.reserve(stop_busses.size());
            for (auto * pbus : stop_busses) {
                vector_stops.push_back(pbus->id);
            }
            std::sort(vector_stops.begin(), vector_stops.end());
            output << "buses";
            for (auto bus_id : vector_stops) {
                output << " " << bus_id;
            }
            output << std::endl;
        }
    }
}

} // namespace work

void StatReader(std::istream & input, std::ostream & output, TransportCatalogue & db) {
    int count = 0;
    input >> count;
    std::string code;
    for (int i = 0; i < count; ++i) {
        input >> code;
        bool is_bus_code = (code == "Bus");
        std::getline(input, code);
        if (is_bus_code) {
            BusId bus_id;
            if (parse::Bus(code, bus_id)) {
                work::Bus(bus_id, output, db);
            }
        } else {
            std::string stop_name;
            if (parse::Stop(code, stop_name)) {
                work::Stop(stop_name, output, db);
            }
        }
    }
}

} // namespace tcatalogue
