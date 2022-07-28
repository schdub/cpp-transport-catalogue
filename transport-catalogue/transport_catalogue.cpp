#include "transport_catalogue.h"
#include <functional>
#include <cassert>

using namespace std;

namespace tcatalogue {

TransportCatalogue::~TransportCatalogue() {
    for (auto & [_, pbus] : buses_) {
        delete pbus;
    }
    buses_.clear();
    for (auto & [_, pstop] : stops_) {
        delete pstop;
    }
    stops_.clear();
}

void TransportCatalogue::AddStop(std::string name, Coordinates coordinates) {
	auto it = stops_.find(name);
	if (it == stops_.end()) {
        Stop* current_stop = new Stop{ move(name), move(coordinates) };
        stops_[current_stop->name] = current_stop;
        stop_to_buses_[current_stop->name] = {};
	} else {
//        LOG() << "update coords for stop '" << name << "'." << std::endl;
        it->second->coordinates = move(coordinates);
	}
}

void TransportCatalogue::AddBus(BusId id, const StopsList & stops, bool is_ring_root) {
    Bus* current_bus = nullptr;
    auto it = buses_.find(id);
    if (it == buses_.end()) {
        current_bus = new Bus{ std::move(id), is_ring_root, {} };
        buses_[current_bus->id] = current_bus;
    } else {
        current_bus = it->second;
        assert(current_bus);
        current_bus->is_ring_root = is_ring_root;
    }
    // build stops list
    current_bus->stops.clear();
    for (auto & stop_name : stops) {
        Stop* current_stop = stops_[stop_name];
        if (current_stop == nullptr) {
//            WARN() << "bus" << id << " stop '" << stop_name << "' not found." << std::endl;
        } else {
            current_bus->stops.push_back(current_stop);
            stop_to_buses_[stop_name].insert(current_bus);
        }
    }
}

const Bus& TransportCatalogue::GetBus(BusId id) const {
    auto it = buses_.find(id);
    if (it == buses_.end() || it->second == nullptr) {
        static Bus empty_bus_information;
        return empty_bus_information;
    }
    return *(it->second);
}

Stop* TransportCatalogue::GetStop(const std::string & stop_name) const {
    auto it = stops_.find(stop_name);
    if (it == stops_.end()) {
        // WARN() << __FUNCTION__ << "stop '" << stop_name << "' not found." << std::endl;
        return nullptr;
    }
    return (it->second);
}

StopBusesOpt TransportCatalogue::GetStopBuses(std::string_view stop_name) const {
    auto it = stop_to_buses_.find(stop_name);
    if (it == stop_to_buses_.end()) {
        // WARN() << __FUNCTION__ << "stop '" << stop_name << "' not found." << std::endl;
        return std::nullopt;
    }
    return StopBusesOpt(it->second);
}

void TransportCatalogue::SetDistanceBetween(const Stop* stopA, const Stop* stopB, size_t value) {
    assert(stopA);
    assert(stopB);
    auto p = std::make_pair(stopA, stopB);
    distance_between_stops_[p] = value;
}

size_t TransportCatalogue::GetDistanceBetween(const Stop* stopA, const Stop* stopB) const {
    assert(stopA);
    assert(stopB);
    auto p = std::make_pair(stopA, stopB);
    auto it = distance_between_stops_.find(p);
    if (it == distance_between_stops_.end()) {
        std::swap(p.first, p.second);
        it = distance_between_stops_.find(p);
        if (it == distance_between_stops_.end()) {
            // WARN() << __FUNCTION__ << "pair of '" << stopA->name << "' and '"
            //                                       << stopB->name << "' not found."
            //                                       << std::endl;
            return 0;
        }
    }
    return it->second;
}

std::size_t TransportCatalogue::DistanceBetweenHash::operator()(const DistanceBetweenKey & p) const noexcept {
    auto hash_function = std::hash<const void*>{};
    std::size_t h1 = hash_function(p.first);
    std::size_t h2 = hash_function(p.second);;
    return h2 * 37 + h1 ;
}

} // namespace tcatalogue
