#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <list>
#include <ostream>
#include <unordered_set>
#include <optional>

#include "geo.h"
#include "domain.h"

namespace tcatalogue {

class TransportCatalogue {
public:
    TransportCatalogue() = default;
    ~TransportCatalogue();

    TransportCatalogue(const TransportCatalogue &) = delete;
    TransportCatalogue& operator=(const TransportCatalogue &) = delete;

    TransportCatalogue(TransportCatalogue &&) = delete;
    TransportCatalogue& operator=(TransportCatalogue &&) = delete;

    void AddStop(std::string name, geo::Coordinates coordinates);
    void AddBus(domain::BusId id, const domain::StopsList & stops, bool is_ring_root);

    const domain::Bus& GetBus(domain::BusId id) const;
    const domain::Bus* GetBusPtr(domain::BusId id) const;

    domain::Stop* GetStop(const std::string & stop_name) const;

    domain::StopBusesOpt GetStopBuses(std::string_view stop_name) const;

    void SetDistanceBetween(const domain::Stop* stopA, const domain::Stop* stopB, size_t value);
    size_t GetDistanceBetween(const domain::Stop* stopA, const domain::Stop* stopB) const;

    std::unordered_set<std::string_view>::const_iterator begin() const;
    std::unordered_set<std::string_view>::const_iterator end() const;

private:
    std::unordered_set<std::string_view> bus_ids_;
    std::unordered_map<std::string_view, domain::Stop*> stops_;
    std::unordered_map<std::string_view, domain::Bus*> buses_;
    std::unordered_map<std::string_view, std::unordered_set<domain::Bus*>> stop_to_buses_;

    using DistanceBetweenKey = std::pair<const domain::Stop*, const domain::Stop*>;
    struct DistanceBetweenHash {
        std::size_t operator()(const DistanceBetweenKey & p) const noexcept;
    };
    std::unordered_map<DistanceBetweenKey, size_t, DistanceBetweenHash> distance_between_stops_;
};

} // namespace tcatalogue
