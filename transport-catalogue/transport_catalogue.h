#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <list>
#include <ostream>
#include <unordered_set>
#include <optional>

#include "geo.h"

namespace tcatalogue {

struct Stop {
    std::string name;
    geo::Coordinates coordinates;
};

using BusId = std::string;
struct Bus {
    BusId id;
    bool is_ring_root;
    std::vector<Stop*> stops;
};

using StopsList = std::list<std::string>;
using StopBusesOpt = std::optional<std::reference_wrapper<const std::unordered_set<Bus*>>>;

class TransportCatalogue {
public:
    TransportCatalogue() = default;
    ~TransportCatalogue();

    TransportCatalogue(const TransportCatalogue &) = delete;
    TransportCatalogue& operator=(const TransportCatalogue &) = delete;

    TransportCatalogue(TransportCatalogue &&) = delete;
    TransportCatalogue& operator=(TransportCatalogue &&) = delete;

    void AddStop(std::string name, geo::Coordinates coordinates);
    void AddBus(BusId id, const StopsList & stops, bool is_ring_root);

    const Bus& GetBus(BusId id) const;

    Stop* GetStop(const std::string & stop_name) const;

    StopBusesOpt GetStopBuses(std::string_view stop_name) const;

    void SetDistanceBetween(const Stop* stopA, const Stop* stopB, size_t value);
    size_t GetDistanceBetween(const Stop* stopA, const Stop* stopB) const;

private:
    std::unordered_map<std::string_view, Stop*> stops_;
    std::unordered_map<std::string_view, Bus*> buses_;
    std::unordered_map<std::string_view, std::unordered_set<Bus*>> stop_to_buses_;

    using DistanceBetweenKey = std::pair<const Stop*, const Stop*>;
    struct DistanceBetweenHash {
        std::size_t operator()(const DistanceBetweenKey & p) const noexcept;
    };
    std::unordered_map<DistanceBetweenKey, size_t, DistanceBetweenHash> distance_between_stops_;
};

} // namespace tcatalogue
