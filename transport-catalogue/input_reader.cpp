#include "input_reader.h"
#include "transport_catalogue.h"

#include <string>
#include <iostream>
#include <vector>
#include <queue>
#include <cstring>
#include <cassert>

namespace tcatalogue {

namespace detail {

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

} // namespace detail {

namespace parse {

bool Stop(const std::string & line, std::string & stop_name, geo::Coordinates & coordinates, std::list<std::pair<std::string, size_t>> & lengths) {
    stop_name.clear();
    lengths.clear();
    auto str_name_and_lat = detail::split(line, ", ");
    if (str_name_and_lat.size() >= 2) {
        auto & s = str_name_and_lat[0];
        auto index = s.find(':');
        if (index != std::string::npos) {
            stop_name = s.substr(1, index - 1);
            coordinates.lat = std::stod( std::string{ detail::trim(s.substr(index + 1)) } );
            coordinates.lng = std::stod( std::string{ detail::trim(str_name_and_lat[1]) } );
            // 3900m to Marushkino
            for (size_t i = 2; i < str_name_and_lat.size(); ++i) {
                auto str_meters_and_name = detail::split(str_name_and_lat[i], "m to ");
                assert(str_meters_and_name.size() == 2);
                std::string other_stop_name{str_meters_and_name[1]};
                size_t meters = std::stoi( std::string{str_meters_and_name[0]} );
                lengths.push_back(std::pair(other_stop_name, meters));
            }
        }
    }
    return !stop_name.empty();
}

bool Bus(std::string_view line, BusId & bus_id, StopsList & stops, bool & is_ring_root) {
    // bus_id
    size_t index_begin = line.find_first_not_of(" \t");
    if (index_begin != std::string::npos) {
        size_t index_end = line.find_first_of(":");
        if (index_end != std::string::npos) {
            bus_id = std::string{line.substr(index_begin, index_end - index_begin)};
            // delemiter
            index_begin = line.find_first_of("->", index_end + 1);
            // parse root
            // " > " - кольцевой A > B > C > D == A B C D A
            // " - " - обычный A - B - C - D == A B C D C B A
            is_ring_root = (line[index_begin] == '>');
            detail::StringList sl = detail::split(line.substr(index_end + 2), is_ring_root ? " > " : " - ");
            stops = std::list<std::string>( sl.begin(), sl.end() );
            return stops.size() != 0;
        }
    }
    return false;
}

} // namespace parse

void InputReader(std::istream & input, TransportCatalogue & db) {
    size_t number_inputs = 0;
    input >> number_inputs;

    std::queue<std::string> pending_busses;
    using LenghtsList = std::list<std::pair<std::string, size_t>>;
    std::unordered_map<std::string, LenghtsList> lengths_for_stop;

    for (size_t i = 0; i < number_inputs; ++i) {
        std::string code;
        input >> code;

        std::string line;
        if (!std::getline(input, line)) {
            break;
        }
        if (code == "Stop") {
            std::string name;
            geo::Coordinates coordinates;
            std::list<std::pair<std::string, size_t>> lengths;
            if (parse::Stop(line, name, coordinates, lengths)) {
                db.AddStop(name, coordinates);
                if (lengths.size() == 0) {
                    // WARN() << __FUNCTION__ << name << " is empty" << std::endl;
                } else {
                    lengths_for_stop[name] = std::move(lengths);
                }
            }
        } else if (code == "Bus") {
            pending_busses.push(line);
        } else {
            // FIXME: probably here should be nagging about users garbige input
        }
    }
    // process pending bus information
    while (pending_busses.empty() == false) {
        BusId bus_id;
        StopsList stops;
        bool is_ring_root;
        std::string line = pending_busses.front();
        if (parse::Bus(line, bus_id, stops, is_ring_root)) {
            db.AddBus(bus_id, stops, is_ring_root);
        }
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

} // namespace tcatalogue
