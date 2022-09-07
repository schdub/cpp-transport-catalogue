#include "domain.h"
#include "transport_catalogue.h"
#include "request_handler.h"
#include <cassert>

using namespace tcatalogue;

namespace domain {

// заполняем транспортную базу информацией из массивов stops и buses.
void FillDatabase(tcatalogue::TransportCatalogue & db, const STOPS & stops, const BUSES & buses) {
    using LenghtsList = std::list<std::pair<std::string, size_t>>;
    std::unordered_map<std::string, LenghtsList> lengths_for_stop;
    for (auto & stop : stops) {
        db.AddStop(stop.stop_name_, stop.coordinates_);
        if (stop.distances_.size() == 0) {
            // WARN() << __FUNCTION__ << name << " is empty" << std::endl;
        } else {
            lengths_for_stop[stop.stop_name_] = std::move(stop.distances_);
        }
    }
    // process bus information
    for (const BUS & bus : buses) {
        db.AddBus(bus.bus_id_, bus.stops_, bus.is_round_trip_);
    }
    // process distances between stops information
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

// заполняем отклики на STAT запросы
void FillStatResponses(const RequestHandler & handler,
                       const STAT_REQUESTS & requests,
                       STAT_RESPONSES & responses) {
    responses.clear();
    const std::string err_not_found("not found");
    for (const STAT_REQUEST & req : requests) {
        if (req.IsStop()) {
            auto opt_stop_busses = handler.GetStopBuses(req.Stop().name_);
            if (opt_stop_busses.has_value() == false) {
                responses.emplace_back( RESP_ERROR{req.id_, err_not_found} );
            } else {
                STAT_RESP_STOP resp;
                resp.request_id = req.id_;
                const auto & stop_busses = opt_stop_busses.value().get();
                if (stop_busses.size() != 0) {
                    std::vector<std::string_view> vector_stops;
                    vector_stops.reserve(stop_busses.size());
                    for (auto * pbus : stop_busses) {
                        vector_stops.push_back(pbus->id);
                    }
                    std::sort(vector_stops.begin(), vector_stops.end());
                    for (const auto & bus_id : vector_stops) {
                        resp.buses.push_back( std::string(bus_id) );
                    }
                }
                responses.emplace_back( resp );
            }
        } else if (req.IsBus()){
            auto bus = handler.GetBus(req.Bus().name_);
            if (bus.stops.empty()) {
                responses.emplace_back( RESP_ERROR{req.id_, err_not_found} );
            } else {
                STAT_RESP_BUS resp;
                resp.request_id = req.id_;
                size_t stops_on_route = (bus.is_round_trip
                     ? bus.stops.size()
                     : (bus.stops.size() * 2) - 1);
                size_t unique_stops = UniqueStopsCount(bus);
                std::pair<double, double> route_length = handler.CalculateRouteLength(bus);
                double curvature = route_length.second / route_length.first;
                resp.curvature         = curvature;
                resp.route_length      = static_cast<int>(route_length.second);
                resp.stop_count        = static_cast<int>(stops_on_route);
                resp.unique_stop_count = static_cast<int>(unique_stops);
                responses.emplace_back(resp);
            }
        } else if (req.IsMap()) {
            STAT_RESP_MAP resp;
            resp.request_id = req.id_;
            resp.map = handler.DrawMap();
            responses.emplace_back(resp);
        } else if (req.IsRoute()) {

        }
    }
}

// подсчитываем уникальные остановки.
size_t UniqueStopsCount(const Bus & bus) {
    std::unordered_set<Stop*> set(bus.stops.begin(), bus.stops.end());
    return set.size();
}

// trim whitespaces from both sides of string
void trim(std::string & s) {
    if (s.empty() == false) { // right
        std::string::iterator p;
        for (p = s.end(); p != s.begin() && ::isspace(*--p););
        if (::isspace(*p) == 0) ++p;
        s.erase(p, s.end());
        if (s.empty() == false) { // left
            for (p = s.begin(); p != s.end() && ::isspace(*p++););
            if (p == s.end() || ::isspace(*p) == 0) --p;
            s.erase(s.begin(), p);
        }
    }
}

// trim whitespaces from both sides of string
std::string_view trim(std::string_view in) {
    auto left = in.begin();
    for (;; ++left) {
        if (left == in.end())
            return std::string_view();
        if (!::isspace(*left)) break;
    }
    auto right = in.end() - 1;
    for (; right > left && ::isspace(*right); --right);
    return std::string_view(left, std::distance(left, right) + 1);
}

// split string using delemiter
using StringList = std::vector<std::string_view>;
StringList split(std::string_view s, const std::string & delimiter) {
    size_t pos_start = 0;
    size_t pos_end;
    size_t delim_len = delimiter.length();
    StringList res;
    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        if (pos_end - pos_start > 0) {
            res.push_back(s.substr(pos_start, pos_end - pos_start));
        }
        pos_start = pos_end + delim_len;
    }
    if (pos_start < s.size())
        res.push_back(s.substr(pos_start));
    return res;
}

} // namespace domain
