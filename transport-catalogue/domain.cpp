#include "domain.h"
#include "transport_catalogue.h"
#include <cassert>

using namespace tcatalogue;

namespace domain {

std::pair<double, double> CalculateRouteLength(const TransportCatalogue & db, const Bus & bus) {
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
    if (bus.is_round_trip == false) {
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
