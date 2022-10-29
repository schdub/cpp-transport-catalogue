#include "serialization.h"

#include "domain.h"

#include <transport_catalogue.pb.h>
#include <cassert>
#include <fstream>

const std::string& Serialization::GetFilePath(const Context &ctx) {
    assert(ctx.serialize_settings.has_value());
    assert(!ctx.serialize_settings.value().file.empty());
    return ctx.serialize_settings.value().file;
}

void svgColorSerialize(const svg::Color & in_color,
                       ::transport_catalogue_pb::Color& out_color) {
    out_color.clear_name();
    out_color.clear_rgb();
    out_color.clear_opacity();
    if (std::holds_alternative<std::string>(in_color)) {
        out_color.set_name(std::get<std::string>(in_color));
    } else if (std::holds_alternative<svg::Rgb>(in_color)) {
        const svg::Rgb & rgb = std::get<svg::Rgb>(in_color);
        int32_t tmp = rgb.red;
        tmp |= rgb.green << 8;
        tmp |= rgb.blue << 16;
        out_color.set_rgb(tmp);
    } else if (std::holds_alternative<svg::Rgba>(in_color)) {
        const svg::Rgba & rgba = std::get<svg::Rgba>(in_color);
        int32_t tmp = rgba.red;
        tmp |= rgba.green << 8;
        tmp |= rgba.blue << 16;
        out_color.set_rgb(tmp);
        out_color.set_opacity(rgba.opacity);
    } else {
        assert(!"This shouldn't happen!!!");
    }
}

void svgColorDeserialize(const ::transport_catalogue_pb::Color & in_color,
                         svg::Color & out_color) {
    if (in_color.has_name()) {
        out_color = in_color.name();
    } else {
        assert( in_color.has_rgb() );
        uint8_t r = in_color.rgb() & 0xff;
        uint8_t g = (in_color.rgb() >> 8)  & 0xff;
        uint8_t b = (in_color.rgb() >> 16) & 0xff;
        if (!in_color.has_opacity()) {
            out_color = svg::Rgb(r, g, b);
        } else {
            out_color = svg::Rgba(r, g, b, in_color.opacity());
        }
    }
}

void Serialization::Write(const Context & context) {
    ::transport_catalogue_pb::Catalogue cat;
    // stops
    for (const domain::STOP & stop : context.stops) {
        ::transport_catalogue_pb::Stop* pbStop = cat.add_stops();
        pbStop->set_name(stop.stop_name_);
        pbStop->set_lat(stop.coordinates_.lat);
        pbStop->set_lng(stop.coordinates_.lng);
        for (const std::pair<std::string, size_t> & pair : stop.distances_) {
            ::transport_catalogue_pb::Distance* pbDistance = pbStop->add_road_distances();
            pbDistance->set_name(pair.first);
            pbDistance->set_distance(pair.second);
        }
    }
    // busses
    for (const domain::BUS & bus : context.busses) {
        ::transport_catalogue_pb::Bus* pbBus = cat.add_buses();
        pbBus->set_bus_id(std::string(bus.bus_id_));
        pbBus->set_is_round_trip(bus.is_round_trip_);
        for (const std::string & stop_name : bus.stops_) {
            pbBus->add_stops(stop_name);
        }
    }
    // routing settings
    if (context.routing_settings.has_value()) {
        const auto & rs = context.routing_settings.value();
        ::transport_catalogue_pb::RoutingSettings* pbRS = cat.mutable_routing_settings();
        pbRS->set_bus_velocity(rs.bus_velocity);
        pbRS->set_bus_wait_time(rs.bus_wait_time);
    }
    // render settings
    if (context.render_settings.has_value()) {
        const auto & rs = context.render_settings.value();
        ::transport_catalogue_pb::RenderSettings* pbRS = cat.mutable_render_settings();
        pbRS->set_width(rs.width);
        pbRS->set_height(rs.height);
        pbRS->set_padding(rs.padding);
        pbRS->set_stop_radius(rs.stop_radius);
        pbRS->set_line_width(rs.line_width);
        pbRS->set_bus_label_font_size(rs.bus_label_font_size);
        pbRS->set_stop_label_font_size(rs.stop_label_font_size);
        pbRS->set_underlayer_width(rs.underlayer_width);

        pbRS->mutable_bus_label_offset()->set_x(rs.bus_label_offset.x);
        pbRS->mutable_bus_label_offset()->set_y(rs.bus_label_offset.y);

        pbRS->mutable_stop_label_offset()->set_x(rs.stop_label_offset.x);
        pbRS->mutable_stop_label_offset()->set_y(rs.stop_label_offset.y);

        svgColorSerialize( rs.underlayer_color, *(pbRS->mutable_underlayer_color()) );

        for (const auto & color : rs.color_palette) {
            auto * pb_color_palette = pbRS->add_color_palette();
            assert(pb_color_palette != nullptr);
            svgColorSerialize(color, *pb_color_palette);
        }
    }

    // output to file stream
    std::ofstream output_file(GetFilePath(context), std::ios::binary);
    cat.SerializeToOstream(&output_file);
}

bool Serialization::Read(Context & context) {
    // input from file stream
    std::ifstream input_file(GetFilePath(context), std::ios::binary);

    ::transport_catalogue_pb::Catalogue cat;
    if (!cat.ParseFromIstream(&input_file)) {
        return false;
    }

    context.stops.clear();
    for (int i = 0; i < cat.stops_size(); ++i) {
        domain::STOP stop;
        const ::transport_catalogue_pb::Stop& ref_stop = cat.stops(i);
        stop.stop_name_ = ref_stop.name();
        stop.coordinates_.lat = ref_stop.lat();
        stop.coordinates_.lng = ref_stop.lng();

        for (int j = 0; j < ref_stop.road_distances_size(); ++j) {
            const ::transport_catalogue_pb::Distance& ref_dist = ref_stop.road_distances(j);
            stop.distances_.emplace_back(ref_dist.name(), ref_dist.distance());
        }
        context.stops.emplace_back(std::move(stop));
    }

    context.busses.clear();
    for (int i = 0; i < cat.buses_size(); ++i) {
        domain::BUS bus;
        const ::transport_catalogue_pb::Bus& ref_bus = cat.buses(i);
        bus.bus_id_ = ref_bus.bus_id();
        bus.is_round_trip_ = ref_bus.is_round_trip();
        for (int j = 0; j < ref_bus.stops_size(); ++j) {
            const std::string & stop_name = ref_bus.stops(j);
            bus.stops_.emplace_back(stop_name);
        }
        context.busses.emplace_back(std::move(bus));
    }

    // routing settings
    if (cat.has_routing_settings()) {
        const auto & pbRS = cat.routing_settings();
        domain::RoutingSettings routing_settings;
        routing_settings.bus_velocity = pbRS.bus_velocity();
        routing_settings.bus_wait_time = pbRS.bus_wait_time();
        context.routing_settings = std::move(routing_settings);
    }

    // render settings
    if (cat.has_render_settings()) {
        const auto & pbRS = cat.render_settings();
        renderer::Settings render_settings;
        render_settings.width = pbRS.width();
        render_settings.height = pbRS.height();
        render_settings.padding = pbRS.padding();
        render_settings.stop_radius = pbRS.stop_radius();
        render_settings.line_width = pbRS.line_width();
        render_settings.bus_label_font_size = pbRS.bus_label_font_size();
        render_settings.stop_label_font_size = pbRS.stop_label_font_size();
        render_settings.underlayer_width = pbRS.underlayer_width();

        render_settings.bus_label_offset.x = pbRS.bus_label_offset().x();
        render_settings.bus_label_offset.y = pbRS.bus_label_offset().y();

        render_settings.stop_label_offset.x = pbRS.stop_label_offset().x();
        render_settings.stop_label_offset.y = pbRS.stop_label_offset().y();

        svgColorDeserialize(pbRS.underlayer_color(), render_settings.underlayer_color);

        for (int i = 0; i < pbRS.color_palette_size(); ++i) {
            svg::Color color;
            svgColorDeserialize(pbRS.color_palette(i), color);
            render_settings.color_palette.emplace_back(std::move(color));
        }
        context.render_settings = std::move(render_settings);
    }
    return true;
}
