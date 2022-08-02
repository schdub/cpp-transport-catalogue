#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <optional>
#include <functional>
#include "geo.h"

/*
 * В этом файле вы можете разместить классы/структуры, которые являются частью предметной области (domain)
 * вашего приложения и не зависят от транспортного справочника. Например Автобусные маршруты и Остановки. 
 *
 * Их можно было бы разместить и в transport_catalogue.h, однако вынесение их в отдельный
 * заголовочный файл может оказаться полезным, когда дело дойдёт до визуализации карты маршрутов:
 * визуализатор карты (map_renderer) можно будет сделать независящим от транспортного справочника.
 *
 * Если структура вашего приложения не позволяет так сделать, просто оставьте этот файл пустым.
 *
 */

namespace tcatalogue {
class TransportCatalogue;
}

namespace domain {

struct Stop {
    std::string name;
    geo::Coordinates coordinates;
};

using BusId = std::string;
struct Bus {
    BusId id;
    bool is_round_trip;
    std::vector<Stop*> stops;
};

using StopsList = std::list<std::string>;
using StopBusesOpt = std::optional<std::reference_wrapper<const std::unordered_set<Bus*>>>;

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
