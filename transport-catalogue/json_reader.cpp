#include "json_reader.h"
#include "transport_catalogue.h"
#include "json.h"

#include <iostream>
#include <cassert>
#include <sstream>

using namespace json;
using namespace domain;
using namespace renderer;

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
        if (req.type_ != "Map") {
            req.name_ = m.at("name").AsString();
        }
        requests.emplace_back(std::move(req));
    }
}

svg::Color FromJsonColor(const json::Array & arr) {
    if (arr.size() == 3) {
        return svg::Rgb(static_cast<int>(arr[0].AsInt()),
                static_cast<int>(arr[1].AsInt()),
                static_cast<int>(arr[2].AsInt()));
    } else if (arr.size() == 4) {
        return svg::Rgba(static_cast<int>(arr[0].AsInt()),
                static_cast<int>(arr[1].AsInt()),
                static_cast<int>(arr[2].AsInt()),
                arr[3].AsDouble());
    }
    assert(!"this shouldn't happen!");
    return svg::Color{};
}

svg::Point FromJsonPoint(const json::Array & arr) {
    assert(arr.size() >= 2);
    svg::Point result;
    result.x = arr[0].AsDouble();
    result.y = arr[1].AsDouble();
    return result;
}

svg::Color FromJsonNodeColor(const json::Node & node) {
    if (node.IsArray()) {
        return FromJsonColor(node.AsArray());
    } else if (node.IsString()) {
        return svg::Color(node.AsString());
    }
    assert(!"FromJsonNodeColor invalid node with color!");
    return svg::Color{};
}

renderer::Settings FromJsonSettings(const json::Dict & dict) {
    Settings result;
//    try {
    result.width                = dict.at("width").AsDouble();
    result.height               = dict.at("height").AsDouble();
    result.padding              = dict.at("padding").AsDouble();
    result.stop_radius          = dict.at("stop_radius").AsDouble();
    result.line_width           = dict.at("line_width").AsDouble();
    result.bus_label_font_size  = dict.at("bus_label_font_size").AsDouble();
    result.bus_label_offset     = FromJsonPoint(dict.at("bus_label_offset").AsArray());
    result.stop_label_font_size = dict.at("stop_label_font_size").AsDouble();
    result.stop_label_offset    = FromJsonPoint(dict.at("stop_label_offset").AsArray());
    result.underlayer_color     = FromJsonNodeColor(dict.at("underlayer_color"));
    result.underlayer_width     = dict.at("underlayer_width").AsDouble();
    for (auto & item : dict.at("color_palette").AsArray()) {
        if (item.IsArray()) {
            result.color_palette.emplace_back(FromJsonColor(item.AsArray()));
        } else if (item.IsString()) {
            result.color_palette.emplace_back(svg::Color(item.AsString()));
        } else {
            assert(!"can't parse color_palette item!");
        }
    }
//    } catch(...) {
//        std::stringstream stream;
//        json::Print(json::Document(dict), stream);
//        throw std::logic_error(stream.str());
//    }

    return result;
}

domain::RoutingSettings FromJsonRouteSettings(const json::Dict & dict) {
    domain::RoutingSettings result;
//    try {
    result.bus_velocity  = dict.at("bus_velocity").AsDouble();
    result.bus_wait_time = dict.at("bus_wait_time").AsDouble();
//    } catch(...) {
//        std::stringstream stream;
//        json::Print(json::Document(dict), stream);
//        throw std::logic_error(stream.str());
//    }

    return result;
}

std::optional<renderer::Settings> JsonReader::ParseRenderSettings() {
    if (!doc_->GetRoot().IsMap()) {
        return std::nullopt;
    }
    const auto & m = doc_->GetRoot().AsMap();
    return FromJsonSettings(m.at("render_settings").AsMap());
}

std::optional<domain::RoutingSettings> JsonReader::ParseRoutingSettings() {
    if (!doc_->GetRoot().IsMap()) {
        return std::nullopt;
    }
    const auto & m = doc_->GetRoot().AsMap();
    return FromJsonRouteSettings(m.at("routing_settings").AsMap());
}

} // namespace tcatalogue
