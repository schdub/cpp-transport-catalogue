#pragma once

#include "domain.h"
#include "map_renderer.h"

class Serialization {
public:
    struct Context {
        domain::BUSES busses;
        domain::STOPS stops;
        std::optional<domain::SerializeSettings> serialize_settings;
        std::optional<renderer::Settings> render_settings;
        std::optional<domain::RoutingSettings> routing_settings;
    };

    static bool Read(Context & context);
    static void Write(const Context & context);

private:
    static const std::string& GetFilePath(const Context& ctx);
};
