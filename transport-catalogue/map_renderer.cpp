#include "map_renderer.h"
#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>
#include <unordered_map>
#include "domain.h"
#include "geo.h"
#include "svg.h"

namespace renderer {

inline const double EPSILON = 1e-6;
bool IsZero(double value) {
    return std::abs(value) < EPSILON;
}

class SphereProjector {
public:
    // points_begin и points_end задают начало и конец интервала элементов geo::Coordinates
    template <typename PointInputIt>
    SphereProjector(PointInputIt points_begin, PointInputIt points_end,
                    double max_width, double max_height, double padding)
        : padding_(padding) //
    {
        // Если точки поверхности сферы не заданы, вычислять нечего
        if (points_begin == points_end) {
            return;
        }

        // Находим точки с минимальной и максимальной долготой
        const auto [left_it, right_it] = std::minmax_element(
            points_begin, points_end,
            [](auto lhs, auto rhs) { return lhs.lng < rhs.lng; });
        min_lon_ = left_it->lng;
        const double max_lon = right_it->lng;

        // Находим точки с минимальной и максимальной широтой
        const auto [bottom_it, top_it] = std::minmax_element(
            points_begin, points_end,
            [](auto lhs, auto rhs) { return lhs.lat < rhs.lat; });
        const double min_lat = bottom_it->lat;
        max_lat_ = top_it->lat;

        // Вычисляем коэффициент масштабирования вдоль координаты x
        std::optional<double> width_zoom;
        if (!IsZero(max_lon - min_lon_)) {
            width_zoom = (max_width - 2 * padding) / (max_lon - min_lon_);
        }

        // Вычисляем коэффициент масштабирования вдоль координаты y
        std::optional<double> height_zoom;
        if (!IsZero(max_lat_ - min_lat)) {
            height_zoom = (max_height - 2 * padding) / (max_lat_ - min_lat);
        }

        if (width_zoom && height_zoom) {
            // Коэффициенты масштабирования по ширине и высоте ненулевые,
            // берём минимальный из них
            zoom_coeff_ = std::min(*width_zoom, *height_zoom);
        } else if (width_zoom) {
            // Коэффициент масштабирования по ширине ненулевой, используем его
            zoom_coeff_ = *width_zoom;
        } else if (height_zoom) {
            // Коэффициент масштабирования по высоте ненулевой, используем его
            zoom_coeff_ = *height_zoom;
        }
    }

    // Проецирует широту и долготу в координаты внутри SVG-изображения
    svg::Point operator()(geo::Coordinates coords) const {
        return {
            (coords.lng - min_lon_) * zoom_coeff_ + padding_,
            (max_lat_ - coords.lat) * zoom_coeff_ + padding_
        };
    }

private:
    double padding_;
    double min_lon_ = 0;
    double max_lat_ = 0;
    double zoom_coeff_ = 0;
};

svg::Document MapRenderer::render(std::list<const domain::Bus*> buses_list) const {
    // получаем координаты всех остановок
    std::unordered_map<const domain::Stop*, geo::Coordinates> screen_coords;
    std::vector<geo::Coordinates> flat_coords;
    for (const domain::Bus* pBus : buses_list) {
        for (const domain::Stop * pStop : pBus->stops) {
            const auto it = screen_coords.find(pStop);
            if (it != screen_coords.end()) continue;
            screen_coords[pStop] = pStop->coordinates;
            flat_coords.push_back(pStop->coordinates);
        }
    }
    const SphereProjector proj{
        flat_coords.begin(),
        flat_coords.end(),
        settings_.width,
        settings_.height,
        settings_.padding
    };
    // Проецируем и выводим координаты
    svg::Document svg_doc;
    for (const domain::Bus* pBus : buses_list) {
        svg::Polyline polyline;
        polyline.SetStrokeWidth(settings_.line_width);
        for (const domain::Stop * pStop : pBus->stops) {
            const auto & geo_coord = screen_coords[pStop];
            const auto & scr_coord = proj(geo_coord);
            polyline.AddPoint(scr_coord);
        }
        svg_doc.Add(polyline);
    }
    return svg_doc;
}

} // namespace renderer {
