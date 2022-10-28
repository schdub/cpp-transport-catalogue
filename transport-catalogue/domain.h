#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <optional>
#include <functional>
#include <variant>
#include <map>
#include "geo.h"

class RequestHandler;

namespace tcatalogue {
class TransportCatalogue;
}

namespace domain {

struct SerializeSettings {
    std::string file;
};

struct RoutingSettings {
    double bus_velocity;
    double bus_wait_time;
};

class Settings {
private:
    Settings() {}

public:
    std::optional<RoutingSettings> routing_settings;
    std::optional<SerializeSettings> serialize_settings;

    static Settings& instance();

    Settings(const Settings &) = delete;
    Settings& operator=(const Settings &) = delete;
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

using BusId = std::string;
struct Bus {
    BusId id;
    bool is_round_trip;
    std::vector<Stop*> stops;
};

using StopsList = std::list<std::string>;
using StopBusesOpt = std::optional<std::reference_wrapper<const std::unordered_set<Bus*>>>;

struct BUS {
    std::string bus_id_;
    StopsList stops_;
    bool is_round_trip_;
};
using BUSES = std::list<BUS>;

void FillDatabase(tcatalogue::TransportCatalogue & db, const STOPS & stops, const BUSES & buses);

// { "id": 1, "type": "Stop", "name": "Ривьерский мост" },

struct STAT_REQ_BUS {
    std::string name_;
};
struct STAT_REQ_STOP {
    std::string name_;
};
struct STAT_REQ_MAP {
};
/*
  "from": "Biryulyovo Zapadnoye",
  "id": 4,
  "to": "Universam",
  "type": "Route"
 */
struct STAT_REQ_ROUTE {
    std::string from_;
    std::string to_;
};

enum class STAT_REQ_TYPE {
    UNKNOWN = 0,
    BUS,
    STOP,
    MAP,
    ROUTE,
};
using STAT_REQUEST_DATA = std::variant<STAT_REQ_BUS, STAT_REQ_STOP, STAT_REQ_MAP, STAT_REQ_ROUTE>;
struct STAT_REQUEST {
    int id_;
    STAT_REQ_TYPE type_;
    STAT_REQUEST_DATA data_;
    void SetType(const std::string & str_type) {
        const static std::map<std::string, STAT_REQ_TYPE> request_types = {
            { "Bus",   STAT_REQ_TYPE::BUS },
            { "Stop",  STAT_REQ_TYPE::STOP },
            { "Map",   STAT_REQ_TYPE::MAP },
            { "Route", STAT_REQ_TYPE::ROUTE },
        };
        static std::map<STAT_REQ_TYPE, STAT_REQUEST_DATA> datas = {
            { STAT_REQ_TYPE::BUS, STAT_REQ_BUS{} },
            { STAT_REQ_TYPE::STOP, STAT_REQ_STOP{} },
            { STAT_REQ_TYPE::MAP, STAT_REQ_MAP{} },
            { STAT_REQ_TYPE::ROUTE, STAT_REQ_ROUTE{} },
        };
        auto it = request_types.find(str_type);
        if (it == request_types.end()) {
            type_ = STAT_REQ_TYPE::UNKNOWN;
        } else {
            type_ = it->second;
            data_ = datas[type_];
        }
    }

    bool IsBus() const {
        return (type_ == STAT_REQ_TYPE::BUS);
    }
    bool IsStop() const {
        return (type_ == STAT_REQ_TYPE::STOP);
    }
    bool IsMap() const {
        return (type_ == STAT_REQ_TYPE::MAP);
    }
    bool IsRoute() const {
        return (type_ == STAT_REQ_TYPE::ROUTE);
    }

    const STAT_REQ_BUS & Bus() const {
        return std::get<STAT_REQ_BUS>(data_);
    }
    const STAT_REQ_STOP & Stop() const {
        return std::get<STAT_REQ_STOP>(data_);
    }
    const STAT_REQ_MAP & Map() const {
        return std::get<STAT_REQ_MAP>(data_);
    }
    const STAT_REQ_ROUTE & Route() const {
        return std::get<STAT_REQ_ROUTE>(data_);
    }

    STAT_REQ_BUS & Bus() {
        return std::get<STAT_REQ_BUS>(data_);
    }
    STAT_REQ_STOP & Stop() {
        return std::get<STAT_REQ_STOP>(data_);
    }
    STAT_REQ_MAP & Map() {
        return std::get<STAT_REQ_MAP>(data_);
    }
    STAT_REQ_ROUTE & Route() {
        return std::get<STAT_REQ_ROUTE>(data_);
    }
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

struct STAT_RESP_ROUTE_ITEM_WAIT {
    std::string stop_name;
    double time = 0.0;
};

struct STAT_RESP_ROUTE_ITEM_BUS {
    std::string bus;
    int span_count = 0;
    double time = 0.0;
};

enum class STAT_RESP_ROUTE_ITEM_TYPE {
    UNKNOWN = 0,
    WAIT,
    BUS,
};

using STAT_RESP_ROUTE_ITEM_DATA = std::variant<STAT_RESP_ROUTE_ITEM_WAIT, STAT_RESP_ROUTE_ITEM_BUS>;
struct STAT_RESP_ROUTE_ITEM {
    STAT_RESP_ROUTE_ITEM_TYPE type_;
    STAT_RESP_ROUTE_ITEM_DATA data_;

    bool IsWait() const {
        return (type_ == STAT_RESP_ROUTE_ITEM_TYPE::WAIT);
    }
    bool IsBus() const {
        return (type_ == STAT_RESP_ROUTE_ITEM_TYPE::BUS);
    }

    STAT_RESP_ROUTE_ITEM_WAIT & Wait() {
        return std::get<STAT_RESP_ROUTE_ITEM_WAIT>(data_);
    }
    STAT_RESP_ROUTE_ITEM_BUS & Bus() {
        return std::get<STAT_RESP_ROUTE_ITEM_BUS>(data_);
    }

    const STAT_RESP_ROUTE_ITEM_WAIT & Wait() const {
        return std::get<STAT_RESP_ROUTE_ITEM_WAIT>(data_);
    }
    const STAT_RESP_ROUTE_ITEM_BUS & Bus() const {
        return std::get<STAT_RESP_ROUTE_ITEM_BUS>(data_);
    }
};
struct STAT_RESP_ROUTE {
    int request_id;
    double total_time = 0.0;
    std::list<STAT_RESP_ROUTE_ITEM> items;
};

using STAT_RESPONSE = std::variant<RESP_ERROR, STAT_RESP_BUS, STAT_RESP_STOP, STAT_RESP_MAP, STAT_RESP_ROUTE>;
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
