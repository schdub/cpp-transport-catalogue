#include "json_reader.h"
#include "transport_catalogue.h"
#include "json.h"

#include <iostream>
#include <cassert>

using namespace json;
using namespace domain;

namespace tcatalogue {

JsonReader::JsonReader(std::istream & input) {
    try {
        doc_ = json::Load(input);
    } catch(...) {
        // WARN() << "can't load json from stream";
        doc_.reset();
    }
}

bool JsonReader::IsOk() const {
    return doc_.has_value();
}

bool WorkStop(const json::Dict & dict, STOP & stop) {
    assert(dict.at("type").AsString() == "Stop");
    //"type": "Stop",
    //"name": "Ривьерский мост",
    //"latitude": 43.587795,
    //"longitude": 39.716901,
    //"road_distances": {"Морской вокзал": 850}
    try {
        stop.stop_name_       = dict.at("name").AsString();
        stop.coordinates_.lat = dict.at("latitude").AsDouble();
        stop.coordinates_.lng = dict.at("longitude").AsDouble();
        stop.distances_.clear();
        for (const auto & [k, v] : dict.at("road_distances").AsMap()) {
            stop.distances_.push_back(std::make_pair(k, v.AsInt()));
        }
    } catch(...) {
        return false;
    }
    return true;
}

bool WorkBus(const json::Dict & dict, BUS & bus) {
    assert(dict.at("type").AsString() == "Bus");
    //"type": "Bus",
    //"name": "114",
    //"stops": ["Морской вокзал", "Ривьерский мост"],
    //"is_roundtrip": false
    try {
        bus.bus_id_        = dict.at("name").AsString();
        bus.is_round_trip_ = dict.at("is_roundtrip").AsBool();
        bus.stops_.clear();
        for (const auto & stop : dict.at("stops").AsArray()) {
            bus.stops_.push_back(stop.AsString());
        }
    } catch(...) {
        return false;
    }
    return bus.stops_.size() != 0;
}

void WorkBaseRequests(const json::Array & arr_base_requests, STOPS & stops, BUSES & buses) {
    for (auto & node : arr_base_requests) {
        if (!node.IsMap()) continue;
        const auto & m = node.AsMap();
        if (m.at("type").AsString() == "Bus") {
            BUS bus;
            if (WorkBus(m, bus)) {
                buses.emplace_back(std::move(bus));
            }
        } else if (m.at("type").AsString() == "Stop") {
            STOP stop;
            if (WorkStop(m, stop)) {
                stops.emplace_back(std::move(stop));
            }
        }
    }
}

void JsonReader::ParseInput(domain::STOPS & stops, domain::BUSES & buses) {
    if (!doc_->GetRoot().IsMap()) {
        //WARN() << "input json doc ill formed." << std::endl;
        return;
    }
    const auto & m = doc_->GetRoot().AsMap();
    WorkBaseRequests(m.at("base_requests").AsArray(), stops, buses);
}

void JsonReader::ParseStatRequests(STAT_REQUESTS & requests) {
    if (!doc_->GetRoot().IsMap()) {
        //WARN() << "input json doc ill formed." << std::endl;
        return;
    }
    const auto & m = doc_->GetRoot().AsMap();
    const json::Array & arr_stat_requests = m.at("stat_requests").AsArray();
    requests.clear();
    for (auto & node : arr_stat_requests) {
        if (!node.IsMap()) continue;
        const auto & m = node.AsMap();
        STAT_REQUEST req;
        req.id_   = m.at("id").AsInt();
        req.type_ = m.at("type").AsString();
        req.name_ = m.at("name").AsString();
        requests.emplace_back(std::move(req));
    }
}

} // namespace tcatalogue
