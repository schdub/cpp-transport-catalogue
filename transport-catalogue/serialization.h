#pragma once

#include "domain.h"
#include "map_renderer.h"

class Serialization {
    static const std::string& GetFilePath();

public:
    struct Context {
        domain::BUSES busses;
        domain::STOPS stops;
        std::optional<renderer::Settings> render_settings;
    };

    static bool Read(Context & context);
    static void Write(const Context & context);
};
