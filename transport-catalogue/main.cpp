#include <fstream>
#include <iostream>
#include <string_view>
#include <iostream>
#include "transport_catalogue.h"
#include "json_reader.h"
#include "request_handler.h"
#include "map_renderer.h"
#include "json_builder.h"
#include "serialization.h"
#include <cassert>

#include "domain.h"

using namespace json;
using namespace domain;
using namespace tcatalogue;
using namespace std::literals;

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

void FillStatResponse(const STAT_RESP_ROUTE & resp, json::Builder & builder) {
    auto array_context = builder.StartDict()
        .Key("request_id").Value(resp.request_id)
        .Key("total_time").Value(resp.total_time)
        .Key("items").StartArray();
    for (const auto & item : resp.items) {
        auto item_dict = array_context.StartDict();
        if (item.IsWait()) {
            const auto & wait_item = item.Wait();
            item_dict.Key("type").Value("Wait");
            item_dict.Key("stop_name").Value(wait_item.stop_name);
            item_dict.Key("time").Value(wait_item.time);
        } else if (item.IsBus()) {
            const auto & bus_item = item.Bus();
            item_dict.Key("type").Value("Bus");
            item_dict.Key("bus").Value(bus_item.bus);
            item_dict.Key("span_count").Value(bus_item.span_count);
            item_dict.Key("time").Value(bus_item.time);
        } else {
            // FIXME: invalid item type!!!
        }
        item_dict.EndDict();
    }
    builder.EndArray().EndDict();
}

void PrintUsage(std::ostream& stream = std::cerr) {
    stream << "Usage: transport_catalogue [make_base|process_requests]\n"sv;
}

int ProcessRequests() {
    JsonReader reader(std::cin);
    if (!reader.IsOk()) {
        return EXIT_FAILURE;
    }

    Serialization::Context context;
    context.serialize_settings = reader.ParseSerializeSettings();
    if (!context.serialize_settings.has_value()) {
        // WARN() << "can't parse serialize_settings!" << std::endl;
        return EXIT_FAILURE;
    }

    TransportCatalogue db; {
        if (!Serialization::Read(context)) {
            // WARN() << "can't parse serialized database!" << std::endl;
            return EXIT_FAILURE;
        }
        FillDatabase(db, context.stops, context.busses);
    }

    STAT_RESPONSES responses; {
        STAT_REQUESTS stat_requests;
        reader.ParseStatRequests(stat_requests);
        if (stat_requests.empty()) {
            //LOG() << "stat_requests is empty." << std::endl;
        } else {
            renderer::MapRenderer drawer(context.render_settings.value());
            RequestHandler handler(db, drawer, context.routing_settings.value());
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

int MakeBase() {
    JsonReader reader(std::cin);
    if (!reader.IsOk()) {
        return EXIT_FAILURE;
    }

    Serialization::Context context;
    context.serialize_settings = reader.ParseSerializeSettings();
    if (!context.serialize_settings.has_value()) {
        // WARN() << "can't parse serialize_settings!" << std::endl;
        return EXIT_FAILURE;
    }

    context.render_settings = reader.ParseRenderSettings();
    if (!context.render_settings.has_value()) {
        // WARN() << "can't parse render_settings!" << std::endl;
        return EXIT_FAILURE;
    }

    context.routing_settings = reader.ParseRoutingSettings();
    if (!context.routing_settings.has_value()) {
        // WARN() << "can't parse routing_settings!" << std::endl;
        return EXIT_FAILURE;
    }

    reader.ParseInput(context.stops, context.busses);
    Serialization::Write(context);
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        PrintUsage();
        return EXIT_FAILURE;
    }
    const std::string_view mode(argv[1]);
    if (mode == "make_base"sv) {
        return MakeBase();
    } else if (mode == "process_requests"sv) {
        return ProcessRequests();
    } else {
        PrintUsage();
        return EXIT_FAILURE;
    }
}

