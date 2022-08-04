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

using namespace svg;
using namespace std;
using namespace domain;

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
    svg::Point operator()(const geo::Coordinates & coords) const {
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

svg::Document MapRenderer::render(std::vector<const domain::Bus*> buses_list) const {
    const std::string FONT_FAMILY{"Verdana"};
    const std::string STOP_POINT_COLOR{"white"};
    const std::string STOP_NAME_COLOR{"black"};
    // сортируем маршруты по имени
    std::sort(buses_list.begin(), buses_list.end(), [](const Bus* lhs, const Bus* rhs) {
        return (lhs->id < rhs->id);
    });
    // получаем географические координаты всех остановок
    std::unordered_map<const domain::Stop*, svg::Point> translated_coords; {
        std::vector<geo::Coordinates> flat_coords;
        for (const domain::Bus* pBus : buses_list) {
            for (const domain::Stop * pStop : pBus->stops) {
                const auto it = translated_coords.find(pStop);
                if (it != translated_coords.end()) continue;
                translated_coords[pStop] = {};
                flat_coords.push_back(pStop->coordinates);
            }
        }
        const SphereProjector projector{
            flat_coords.begin(),
            flat_coords.end(),
            settings_.width,
            settings_.height,
            settings_.padding
        };
        // и транслируем из сферических в 2D координаты
        for (auto & [pStop, points] : translated_coords) {
            points = std::move(projector(pStop->coordinates));
        }
    }

    // формируем svg документ
    svg::Document svg_doc;
    size_t current_color_idx = 0;
    size_t current_color_max = settings_.color_palette.size();
    assert(current_color_max > 0);

    // выводим линии маршрутов
    {
        for (const domain::Bus* pBus : buses_list) {
            svg::Polyline rootline;
            rootline.SetFillColor("none")
                    .SetStrokeColor(settings_.color_palette.at(current_color_idx))
                    .SetStrokeWidth(settings_.line_width)
                    .SetStrokeLineCap(StrokeLineCap::ROUND)
                    .SetStrokeLineJoin(StrokeLineJoin::ROUND);
            // дорога "туда"
            for (const domain::Stop * pStop : pBus->stops) {
                const auto & scr_coord = translated_coords[pStop];
                rootline.AddPoint(scr_coord);
            }
            // обратная дорога для не кольцевого маршрута
            if (pBus->is_round_trip == false && pBus->stops.size() > 1) {
                auto index = pBus->stops.end();
                index -= 2;
                for (; index >= pBus->stops.begin(); --index) {
                    const auto & scr_coord = translated_coords[*index];
                    rootline.AddPoint(scr_coord);
                }
            }
            svg_doc.Add(rootline);
            // выбираем следующий цвет в палитре
            if (++current_color_idx >= current_color_max) {
                current_color_idx = 0;
            }
        }
    }
    // названия маршрутов
    {
        current_color_idx = 0;
        svg::Text utext;
        svg::Text text;
        utext.SetFillColor(settings_.underlayer_color)
             .SetStrokeColor(settings_.underlayer_color)
             .SetStrokeWidth(settings_.underlayer_width)
             .SetStrokeLineCap(StrokeLineCap::ROUND)
             .SetStrokeLineJoin(StrokeLineJoin::ROUND);
        text.SetOffset(settings_.bus_label_offset)
            .SetFontSize(settings_.bus_label_font_size)
            .SetFontFamily(FONT_FAMILY)
            .SetFontWeight("bold");
        utext.SetOffset(settings_.bus_label_offset)
            .SetFontSize(settings_.bus_label_font_size)
            .SetFontFamily(FONT_FAMILY)
            .SetFontWeight("bold");
        for (const domain::Bus* pBus : buses_list) {
            utext.SetData(std::string(pBus->id));
            text.SetData(std::string(pBus->id));
            text.SetFillColor(settings_.color_palette.at(current_color_idx));
            {
                const auto & points = translated_coords[ pBus->stops.front() ];
                text.SetPosition(points);
                utext.SetPosition(points);
            }
            if (pBus->is_round_trip) {
                svg_doc.Add(utext);
                svg_doc.Add(text);
            } else {
                // для некольцевого маршрута пометим начало
                svg_doc.Add(utext);
                svg_doc.Add(text);
                // и окончание маршрута
                if (pBus->stops.front() != pBus->stops.back()) {
                    const auto & points = translated_coords[ pBus->stops.back() ];
                    text.SetPosition(points);
                    utext.SetPosition(points);
                    svg_doc.Add(utext);
                    svg_doc.Add(text);
                }
            }
            // выбираем следующий цвет в палитре
            if (++current_color_idx >= current_color_max) {
                current_color_idx = 0;
            }
        }
    }

    // отсортируем названия остановок
    std::vector<const domain::Stop*> sorted_stops;
    sorted_stops.reserve(translated_coords.size());
    for (const auto & [pStop, pts] : translated_coords) {
        sorted_stops.push_back(pStop);
    }
    std::sort(sorted_stops.begin(), sorted_stops.end(),
        [](const domain::Stop* lhs, const domain::Stop* rhs) {
        return (lhs->name < rhs->name);
    });

    // кружки остановок
    {
        svg::Circle circle;
        circle.SetFillColor(STOP_POINT_COLOR)
              .SetRadius( settings_.stop_radius );
        for (const Stop * pStop : sorted_stops) {
            const svg::Point & pts = translated_coords[pStop];
            circle.SetCenter( pts );
            svg_doc.Add(circle);
        }
    }
    // названия остановок
    {
        svg::Text utext;
        svg::Text text;
        text.SetFillColor(STOP_NAME_COLOR);
        utext.SetFillColor(settings_.underlayer_color)
            .SetStrokeColor(settings_.underlayer_color)
            .SetStrokeWidth(settings_.underlayer_width)
            .SetStrokeLineCap(StrokeLineCap::ROUND)
            .SetStrokeLineJoin(StrokeLineJoin::ROUND);
        text.SetOffset(settings_.stop_label_offset)
            .SetFontSize(settings_.stop_label_font_size)
            .SetFontFamily(FONT_FAMILY);
        utext.SetOffset(settings_.stop_label_offset)
             .SetFontSize(settings_.stop_label_font_size)
             .SetFontFamily(FONT_FAMILY);
        for (const Stop * pStop : sorted_stops) {
            const svg::Point & pts = translated_coords[pStop];
            utext.SetPosition(pts);
            text.SetPosition(pts);
            utext.SetData(std::string(pStop->name));
            text.SetData(std::string(pStop->name));
            svg_doc.Add(utext);
            svg_doc.Add(text);
        }
    }
    return svg_doc;
}

} // namespace renderer {
