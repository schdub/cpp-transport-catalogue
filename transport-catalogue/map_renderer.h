#pragma once
#include "svg.h"
#include "transport_catalogue.h"
#include <vector>

namespace renderer {

struct Settings {
    double width;
    double height;
    double padding;
    double stop_radius;
    double line_width;
    double bus_label_font_size;
    svg::Point bus_label_offset;
    double stop_label_font_size;
    svg::Point stop_label_offset;
    svg::Color underlayer_color;
    double underlayer_width;
    std::vector<svg::Color> color_palette;
};

class MapRenderer {
    const Settings & settings_;
public:
    MapRenderer(const Settings & settings) : settings_(settings) {}
    svg::Document render(std::list<const domain::Bus *> buses_list) const;
};

}
