#include <iostream>
#include "transport_catalogue.h"
#include "json_reader.h"
#include <cassert>
#include <queue>

using namespace json;
using namespace domain;
using namespace tcatalogue;

struct STOP {
    std::string stop_name_;
    geo::Coordinates coordinates_;
    std::list<std::pair<std::string, size_t>> distances_;
};

bool WorkStop(const json::Dict & m, STOP & stop) {
    assert(m.at("type").AsString() == "Stop");
    //"type": "Stop",
    //"name": "Ривьерский мост",
    //"latitude": 43.587795,
    //"longitude": 39.716901,
    //"road_distances": {"Морской вокзал": 850}
    try {
        stop.stop_name_       = m.at("name").AsString();
        stop.coordinates_.lat = m.at("latitude").AsDouble();
        stop.coordinates_.lng = m.at("longitude").AsDouble();
        stop.distances_.clear();
        for (const auto & [k, v] : m.at("road_distances").AsMap()) {
            stop.distances_.push_back(std::make_pair(k, v.AsInt()));
        }
    } catch(...) {
        return false;
    }
    return true;
}

struct BUS {
    BusId bus_id_;
    StopsList stops_;
    bool is_round_trip_;
};

bool WorkBus(const json::Dict & m, BUS & bus) {
    assert(m.at("type").AsString() == "Bus");
    //"type": "Bus",
    //"name": "114",
    //"stops": ["Морской вокзал", "Ривьерский мост"],
    //"is_roundtrip": false
    try {
        bus.bus_id_        = m.at("name").AsString();
        bus.is_round_trip_ = m.at("is_roundtrip").AsBool();
        bus.stops_.clear();
        for (const auto & stop : m.at("stops").AsArray()) {
            bus.stops_.push_back(stop.AsString());
        }
    } catch(...) {
        return false;
    }
    return bus.stops_.size() != 0;
}

void WorkBaseRequests(TransportCatalogue & db, const json::Array & arr_base_requests) {
    std::queue<BUS> pending_busses;
    using LenghtsList = std::list<std::pair<std::string, size_t>>;
    std::unordered_map<std::string, LenghtsList> lengths_for_stop;
    for (auto & node : arr_base_requests) {
        if (!node.IsMap()) continue;
        const auto & m = node.AsMap();
        if (m.at("type").AsString() == "Bus") {
            BUS bus;
            if (WorkBus(m, bus)) {
                pending_busses.push(bus);
            }
        } else if (m.at("type").AsString() == "Stop") {
            STOP stop;
            if (WorkStop(m, stop)) {
                db.AddStop(stop.stop_name_, stop.coordinates_);
                if (stop.distances_.size() == 0) {
                    // WARN() << __FUNCTION__ << name << " is empty" << std::endl;
                } else {
                    lengths_for_stop[stop.stop_name_] = std::move(stop.distances_);
                }
            }
        }
    }
    // process pending bus information
    while (pending_busses.empty() == false) {
        const BUS & bus = pending_busses.front();
        db.AddBus(bus.bus_id_, bus.stops_, bus.is_round_trip_);
        pending_busses.pop();
    }
    // process pending lenghts between stops information
    for (auto & [stop_name, lenghts] : lengths_for_stop) {
        const Stop* pstopA = db.GetStop(stop_name);
        if (pstopA == nullptr) {
            // WARN() << __FUNCTION__ << " [A] '" << stop_name << "' not found." << std::endl;
            continue;
        }
        for (auto & [other_stop_name, meters] : lenghts) {
            const Stop* pstopB = db.GetStop(other_stop_name);
            if (pstopB == nullptr) {
                // WARN() << __FUNCTION__ << " [B] '" << other_stop_name << "' not found." << std::endl;
                continue;
            }
            db.SetDistanceBetween(pstopA, pstopB, meters);
        }
    }
}

// { "id": 1, "type": "Stop", "name": "Ривьерский мост" },
struct STAT_REQUEST {
    int id_;
    std::string type_;
    std::string name_;
};

std::list<STAT_REQUEST> WorkStatRequests(TransportCatalogue & db, const json::Array & arr_stat_requests) {
    std::list<STAT_REQUEST> requests;
    for (auto & node : arr_stat_requests) {
        if (!node.IsMap()) continue;
        const auto & m = node.AsMap();
        STAT_REQUEST req;
        req.id_   = m.at("id").AsInt();
        req.type_ = m.at("type").AsString();
        req.name_ = m.at("name").AsString();
        requests.push_back(req);
    }
    return requests;
}

void FillBusResponse(const TransportCatalogue & db, const STAT_REQUEST & request, json::Dict & out_dict) {
    auto bus = db.GetBus(request.name_);
    if (bus.stops.empty()) {
        out_dict["error_message"] = "not found";
    } else {
        size_t stops_on_route = (bus.is_round_trip
             ? bus.stops.size()
             : (bus.stops.size() * 2) - 1);
        size_t unique_stops = UniqueStopsCount(bus);
        std::pair<double, double> route_length = CalculateRouteLength(db, bus);
        double curvature = route_length.second / route_length.first;
        out_dict["curvature"]         = curvature;
        out_dict["route_length"]      = static_cast<int>(route_length.second);
        out_dict["stop_count"]        = static_cast<int>(stops_on_route);
        out_dict["unique_stop_count"] = static_cast<int>(unique_stops);
    }
}

void FillStopResponse(const TransportCatalogue & db, const STAT_REQUEST & request, json::Dict & out_dict) {
    auto opt_stop_busses = db.GetStopBuses(request.name_);
    if (opt_stop_busses.has_value() == false) {
        out_dict["error_message"] = "not found";
    } else {
        const auto & stop_busses = opt_stop_busses.value().get();
        json::Array arr_busses;
        if (stop_busses.size() != 0) {
            std::vector<std::string_view> vector_stops;
            vector_stops.reserve(stop_busses.size());
            for (auto * pbus : stop_busses) {
                vector_stops.push_back(pbus->id);
            }
            std::sort(vector_stops.begin(), vector_stops.end());
            for (const auto & bus_id : vector_stops) {
                arr_busses.push_back( std::string(bus_id) );
            }
        }
        out_dict["buses"] = arr_busses;
    }
}

int main() {
    /*
     * Примерная структура программы:
     *
     * Считать JSON из stdin
     * Построить на его основе JSON базу данных транспортного справочника
     * Выполнить запросы к справочнику, находящиеся в массиве "stat_requests", построив JSON-массив
     * с ответами.
     * Вывести в stdout ответы в виде JSON
     */
    std::optional<json::Document> doc = JsonReader(std::cin);
    std::optional<std::list<STAT_REQUEST>> stat_requests;
    TransportCatalogue db;
    if (doc) {
        if (doc->GetRoot().IsMap()) {
            const auto & m = doc->GetRoot().AsMap();
            WorkBaseRequests(db, m.at("base_requests").AsArray());
            stat_requests = WorkStatRequests(db, m.at("stat_requests").AsArray());
        }
    }

    if (stat_requests) {
        json::Array array;
        for (const STAT_REQUEST & request : (*stat_requests)) {
            json::Dict dictionary;
            dictionary["id"] = request.id_;
            if (request.type_ == "Bus") {
                FillBusResponse(db, request, dictionary);
            } else if (request.type_ == "Stop") {
                FillStopResponse(db, request, dictionary);
            }
            array.emplace_back(dictionary);
        }
        json::Print(json::Document(array), std::cout);
    }
}
