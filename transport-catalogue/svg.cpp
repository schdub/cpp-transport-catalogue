#include "svg.h"
#include <string>
#include <vector>
#include <cassert>

namespace detail {
    void replace(
        std::string & s,
        const std::string & a,
        const std::string & b,
        size_t begin = 0
    ) {
        for (size_t idx = begin ;; idx += b.length()) {
            idx = s.find(a, idx);
            if (idx == std::string::npos) break;
            s.replace(idx, a.length(), b);
        }
    }
}

namespace svg {

using namespace std::literals;

std::ostream& operator<<(std::ostream& out, const StrokeLineCap & line_cap) {
    switch (line_cap) {
    case   StrokeLineCap::BUTT: out << "butt";   break;
    case  StrokeLineCap::ROUND: out << "round";  break;
    case StrokeLineCap::SQUARE: out << "square"; break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const StrokeLineJoin & line_join) {
    switch (line_join) {
    case       StrokeLineJoin::ARCS: out << "arcs";       break;
    case      StrokeLineJoin::BEVEL: out << "bevel";      break;
    case      StrokeLineJoin::MITER: out << "miter";      break;
    case StrokeLineJoin::MITER_CLIP: out << "miter-clip"; break;
    case      StrokeLineJoin::ROUND: out << "round";      break;
    }
    return out;
}

std::ostream& operator<< (std::ostream& out, const Rgb & color) {
    out << "rgb(" << static_cast<int>(color.red) << ","
                  << static_cast<int>(color.green) << ","
                  << static_cast<int>(color.blue) << ")";
    return out;
}

std::ostream& operator<< (std::ostream& out, const Rgba & color) {
    out << "rgba(" << static_cast<int>(color.red) << ","
                   << static_cast<int>(color.green) << ","
                   << static_cast<int>(color.blue) << ","
                   << color.opacity << ")";
    return out;
}


class ColorPrinter {
    std::ostream& out_;
public:
    ColorPrinter(std::ostream & out)
        : out_(out)
    {}
    void operator()(std::monostate) const { out_ << NoneColor; }
    void operator()(const Rgb & color) const  { out_ << color; }
    void operator()(const Rgba & color) const { out_ << color; }
    void operator()(const std::string & color) const { out_ << color; }
};


std::ostream& operator<< (std::ostream& out, const Color & color) {
    std::visit(ColorPrinter(out), color);
    return out;
}

void Object::Render(const RenderContext& context) const {
    context.RenderIndent();

    // Делегируем вывод тега своим подклассам
    RenderObject(context);

    context.out << std::endl;
}

// ---------- Circle ------------------

Circle& Circle::SetCenter(Point center)  {
    center_ = std::move(center);
    return *this;
}

Circle& Circle::SetRadius(double radius)  {
    radius_ = radius;
    return *this;
}

void Circle::RenderObject(const RenderContext& context) const {
    auto& out = context.out;
    out << "<circle cx=\""sv << center_.x << "\" cy=\""sv << center_.y << "\" "sv;
    out << "r=\""sv << radius_ << "\""sv;
    RenderAttrs(out);
    out << " />"sv;
}

// ---------- Polyline ------------------

// Добавляет очередную вершину к ломаной линии
Polyline& Polyline::AddPoint(Point point) {
    points_.emplace_back(std::move(point));
    return *this;
}

void Polyline::RenderObject(const RenderContext& context) const {
    // <polyline points="0,100 50,25 50,75 100,0" />
    auto & out = context.out;
    out << "<polyline";
    out << " points=\"";
    if (points_.size() != 0) {
        for (auto it = points_.begin();;) {
            out << (*it).x << "," << (*it).y;
            if (++it == points_.end()) {
                break;
            }
            out << " ";
        }
    }
    out << "\"";
    RenderAttrs(out);
    out << "/>";
}

// ---------- Text ------------------

Text& Text::SetPosition(Point pos) {
    pos_ = std::move(pos);
    return *this;
}

// Задаёт смещение относительно опорной точки (атрибуты dx, dy)
Text& Text::SetOffset(Point offset) {
    offset_ = std::move(offset);
    return *this;
}

// Задаёт размеры шрифта (атрибут font-size)
Text& Text::SetFontSize(uint32_t size) {
    font_size_ = std::move(size);
    return *this;
}

// Задаёт название шрифта (атрибут font-family)
Text& Text::SetFontFamily(std::string font_family) {
    font_family_ = std::move(font_family);
    return *this;
}

// Задаёт толщину шрифта (атрибут font-weight)
Text& Text::SetFontWeight(std::string font_weight) {
    font_weight_ = std::move(font_weight);
    return *this;
}

// Задаёт текстовое содержимое объекта (отображается внутри тега text)
Text& Text::SetData(std::string data) {
    std::vector<std::string> from = {
        "\"", "<", ">", "'", "&"
    };
    std::vector<std::string> to = {
       "&quot;", "&apos;", "&lt;", "&gt;", "&amp;"
    };
    assert(from.size() == to.size());
    for (size_t i = 0; i < from.size(); ++i) {
        detail::replace(data, from[i], to[i]);
    }
    data_ = std::move(data);
    return *this;
}

void Text::RenderObject(const RenderContext& context) const {
    // <text x="20" y="35" class="small">My</text>
    // if (data_.empty()) return;
    auto & out = context.out;
    out << "<text";
    // attributes
    RenderAttrs(out);
    // pos_
    out << " x=\"" << pos_.x << "\"";
    out << " y=\"" << pos_.y << "\"";
    // offset_
    out << " dx=\"" << offset_.x << "\"";
    out << " dy=\"" << offset_.y << "\"";
    // font_size_
    out << " font-size=\"" << font_size_ << "\"";
    // font_family_
    if (!font_family_.empty())
    out << " font-family=\"" << font_family_ << "\"";
    // font_weight_
    if (!font_weight_.empty())
    out << " font-weight=\"" << font_weight_ << "\"";
    // data_
    out << ">" << data_ << "</text>";
}

/*
 Метод Add добавляет в svg-документ любой объект-наследник svg::Object.
 Пример использования:
 Document doc;
 doc.Add(Circle().SetCenter({20, 30}).SetRadius(15));
*/

// Добавляет в svg-документ объект-наследник svg::Object
void Document::AddPtr(std::unique_ptr<Object>&& obj){
    objs_.emplace_back(std::move(obj));
}

// Выводит в ostream svg-представление документа
void Document::Render(std::ostream& out) const {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"sv << std::endl;
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">"sv << std::endl;
    RenderContext ctx(std::cout, 2, 2);
    for (const auto & pobj : objs_) {
        pobj->Render(ctx);
    }
    out << "</svg>"sv;
}

}  // namespace svg
