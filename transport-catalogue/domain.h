#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <optional>
#include <functional>
#include "geo.h"

namespace tcatalogue {
class TransportCatalogue;
}

namespace domain {

struct Stop {
    std::string name;
    geo::Coordinates coordinates;
};

struct STOP {
    std::string stop_name_;
    geo::Coordinates coordinates_;
    std::list<std::pair<std::string, size_t>> distances_;
};

using BusId = std::string;
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
