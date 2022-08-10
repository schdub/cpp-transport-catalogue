#include <iostream>
#include "transport_catalogue.h"
#include "json_reader.h"
#include "request_handler.h"
#include "map_renderer.h"
#include "json_builder.h"
#include <cassert>

#include "domain.h"

using namespace json;
using namespace domain;
using namespace tcatalogue;

void FillStatResponse(const RESP_ERROR & resp, json::Builder & builder) {
    builder.StartDict()
        .Key("request_id").Value(resp.request_id)
        .Key("error_message").Value(resp.error_message)
        .EndDict();
}

void FillStatResponse(const STAT_RESP_BUS & resp, json::Builder & builder) {
    builder.StartDict()
        .Key("request_id").Value(resp.request_id)
        .Key("curvature").Value(resp.curvature)
        .Key("route_length").Value(resp.route_length)
        .Key("stop_count").Value(resp.stop_count)
        .Key("unique_stop_count").Value(resp.unique_stop_count)
        .EndDict();
}

void FillStatResponse(const STAT_RESP_STOP & resp, json::Builder & builder) {
    builder.StartDict()
        .Key("request_id").Value(resp.request_id)
        .Key("buses").StartArray();
    for (const auto & bus_name : resp.buses) {
        builder.Value(std::string(bus_name));
    }
    builder.EndArray().EndDict();
}

void FillStatResponse(const STAT_RESP_MAP & resp, json::Builder & builder) {
    builder.StartDict()
        .Key("request_id").Value(resp.request_id)
        .Key("map").Value(std::move(resp.map))
        .EndDict();
}

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

    auto opt_renderer_settings = reader.ParseRenderSettings();
    if (!opt_renderer_settings.has_value()) {
        // WARN() << "can't parse render_settings!" << std::endl;
        return EXIT_FAILURE;
    }

    STAT_RESPONSES responses; {
        STAT_REQUESTS stat_requests;
        reader.ParseStatRequests(stat_requests);
        if (stat_requests.empty()) {
            //LOG() << "stat_requests is empty." << std::endl;
        } else {
            renderer::MapRenderer drawer(opt_renderer_settings.value());
            RequestHandler handler(db, drawer);
            FillStatResponses(handler, stat_requests, responses);
        }
    }

    if (responses.empty()) {
        //LOG() << "responses is empty." << std::endl;
    } else {
        json::Builder builder;
        builder.StartArray();
        for (const STAT_RESPONSE & resp : responses) {
            std::visit([&builder](const auto & stat_response) {
                FillStatResponse(stat_response, builder);
            }, resp);
        }
        builder.EndArray();

        json::Print(json::Document(builder.Build()), std::cout);
    }

    return EXIT_SUCCESS;
}
