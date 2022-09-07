#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <optional>
#include <functional>
#include <variant>
#include "geo.h"

class RequestHandler;

namespace tcatalogue {
class TransportCatalogue;
}

namespace domain {

struct RoutingSettings {
    double bus_velocity;
    double bus_wait_time;
};

struct Stop {
    std::string name;
    geo::Coordinates coordinates;
};

struct STOP {
    std::string stop_name_;
    geo::Coordinates coordinates_;
    std::list<std::pair<std::string, size_t>> distances_;
};
using STOPS = std::list<STOP>;

using BusId = std::string_view;
struct Bus {
    BusId id;
    bool is_round_trip;
    std::vector<Stop*> stops;
};

using StopsList = std::list<std::string>;
using StopBusesOpt = std::optional<std::reference_wrapper<const std::unordered_set<Bus*>>>;

struct BUS {
    BusId bus_id_;
    StopsList stops_;
    bool is_round_trip_;
};
using BUSES = std::list<BUS>;

void FillDatabase(tcatalogue::TransportCatalogue & db, const STOPS & stops, const BUSES & buses);

// { "id": 1, "type": "Stop", "name": "Ривьерский мост" },
struct STAT_REQUEST {
    int id_;
    std::string type_;
    std::string name_;
};
using STAT_REQUESTS = std::list<STAT_REQUEST>;

struct RESP_ERROR {
    int request_id;
    std::string error_message;
};
struct STAT_RESP_BUS {
    int request_id;
    double curvature;
    int route_length;
    int stop_count;
    int unique_stop_count;
};
struct STAT_RESP_STOP {
    int request_id;
    std::vector<std::string> buses;
};
struct STAT_RESP_MAP {
    int request_id;
    std::string map;
};

using STAT_RESPONSE = std::variant<RESP_ERROR, STAT_RESP_BUS, STAT_RESP_STOP, STAT_RESP_MAP>;
using STAT_RESPONSES = std::list<STAT_RESPONSE>;
void FillStatResponses(const RequestHandler & handler, const STAT_REQUESTS & requests, STAT_RESPONSES & responses);

std::pair<double, double> CalculateRouteLength(const tcatalogue::TransportCatalogue & db, const Bus & bus);

size_t UniqueStopsCount(const Bus & bus);

// trim whitespaces from both sides of string
void trim(std::string & s);

// trim whitespaces from both sides of string
std::string_view trim(std::string_view in);

// split string using delemiter
using StringList = std::vector<std::string_view>;
StringList split(std::string_view s, const std::string & delimiter);

template<typename T>
void replace(std::string & str, const T & from, const T & to, size_t begin = 0) {
    for (size_t idx = begin ;; idx += to.length()) {
        idx = str.find(from, idx);
        if (idx == std::string::npos) break;
        str.replace(idx, from.length(), to);
    }
}

} // namespace domain
