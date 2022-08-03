#include <iostream>
#include "transport_catalogue.h"
#include "json_reader.h"
#include "request_handler.h"
#include "map_renderer.h"
#include <cassert>

#include "domain.h"

#define DISABLE_STAT_REQUESTS 1

using namespace json;
using namespace domain;
using namespace tcatalogue;

#if (DISABLE_STAT_REQUESTS == 0)
void FillStatResponse(const RESP_ERROR & resp, json::Dict & out_dict) {
    out_dict["request_id"]    = resp.request_id;
    out_dict["error_message"] = resp.error_message;
}
void FillStatResponse(const STAT_RESP_BUS & resp, json::Dict & out_dict) {
    out_dict["request_id"]        = resp.request_id;
    out_dict["curvature"]         = resp.curvature;
    out_dict["route_length"]      = resp.route_length;
    out_dict["stop_count"]        = resp.stop_count;
    out_dict["unique_stop_count"] = resp.unique_stop_count;
}
void FillStatResponse(const STAT_RESP_STOP & resp, json::Dict & out_dict) {
    out_dict["request_id"] = resp.request_id;
    json::Array arr_buses;
    for (const auto & bus_name : resp.buses) {
        arr_buses.emplace_back( std::string(bus_name) );
    }
    out_dict["buses"] = std::move(arr_buses);
}
#else
#endif

int main() {
    JsonReader reader(std::cin);
    if (!reader.IsOk()) {
        return EXIT_FAILURE;
    }

    TransportCatalogue db; {
        STOPS stops;
        BUSES buses;
        reader.ParseInput(stops, buses);
        FillDatabase(db, stops, buses);
    }

#if (DISABLE_STAT_REQUESTS == 1)
    auto opt_renderer_settings = reader.ParseRenderSettings();
    if (opt_renderer_settings.has_value()) {
        RequestHandler handler(db);
        renderer::MapRenderer rend(opt_renderer_settings.value());
        svg::Document doc = rend.render(handler.GetAllBuses());
        doc.Render(std::cout);
    }
#else
    STAT_RESPONSES responses; {
        STAT_REQUESTS stat_requests;
        reader.ParseStatRequests(stat_requests);
        if (!stat_requests.empty()) {
            FillStatResponses(db, stat_requests, responses);
        }
    }
    if (!responses.empty()) {
        json::Array array;
        for (const STAT_RESPONSE & resp : responses) {
            json::Dict dictionary;
            std::visit([&dictionary](const auto & stat_response) {
                FillStatResponse(stat_response, dictionary);
            }, resp);
            array.emplace_back(dictionary);
        }
        json::Print(json::Document(array), std::cout);
    }
#endif

    return EXIT_SUCCESS;
}
