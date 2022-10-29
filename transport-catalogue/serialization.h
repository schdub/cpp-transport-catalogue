#pragma once

#include "domain.h"
#include "map_renderer.h"

class Serialization {
    static const std::string& GetFilePath();

public:
    static bool Read(domain::BUSES & busses,
                     domain::STOPS & stops,
                     renderer::Settings & render_settings);
    static void Write(const domain::BUSES & busses,
                      const domain::STOPS & stops,
                      const std::optional<renderer::Settings> & opt_render_settings);
};
