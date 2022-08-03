#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <list>
#include <optional>
#include <variant>

namespace svg {

struct Rgb {
    uint8_t red{};
    uint8_t green{};
    uint8_t blue{};
    Rgb() = default;
    Rgb(uint8_t red, uint8_t green, uint8_t blue)
        : red(red)
        , green(green)
        , blue(blue)
    {}
};

struct Rgba {
    uint8_t red{};
    uint8_t green{};
    uint8_t blue{};
    double opacity{1.0};
    Rgba() = default;
    Rgba(uint8_t red, uint8_t green, uint8_t blue, double opacity)
        : red(red)
        , green(green)
        , blue(blue)
        , opacity(opacity)
    {}
};

using Color = std::variant<std::monostate, Rgb, Rgba, std::string>;
std::ostream& operator<< (std::ostream& out, const Color &);
inline const Color NoneColor{"none"};

enum class StrokeLineCap {
    BUTT,
    ROUND,
    SQUARE,
};

enum class StrokeLineJoin {
    ARCS,
    BEVEL,
    MITER,
    MITER_CLIP,
    ROUND,
};

std::ostream& operator<< (std::ostream& out, const StrokeLineCap & line_cap);
std::ostream& operator<< (std::ostream& out, const StrokeLineJoin & line_cap);

template <typename Owner>
class PathProps {
    std::optional<Color> fill_color_;
    std::optional<Color> stroke_color_;
    std::optional<double> stroke_width_;
    std::optional<StrokeLineCap> stroke_linecap_;
    std::optional<StrokeLineJoin> stroke_linejoin_;

    Owner& AsOwner() {
        // static_cast безопасно преобразует *this к Owner&,
        // если класс Owner — наследник PathProps
        return static_cast<Owner&>(*this);
    }

public:
    Owner& SetFillColor(Color color) {
        fill_color_ = std::move(color);
        return AsOwner();
    }
    Owner& SetStrokeColor(Color color) {
        stroke_color_ = std::move(color);
        return AsOwner();
    }
    Owner& SetStrokeWidth(double stroke_width) {
        stroke_width_ = std::move(stroke_width);
        return AsOwner();
    }
    Owner& SetStrokeLineCap(StrokeLineCap stroke_linecap) {
        stroke_linecap_ = std::move(stroke_linecap);
        return AsOwner();
    }
    Owner& SetStrokeLineJoin(StrokeLineJoin stroke_linejoin) {
        stroke_linejoin_ = std::move(stroke_linejoin);
        return AsOwner();
    }

protected:
    ~PathProps() = default;

    void RenderAttrs(std::ostream& out) const {
        using namespace std::literals;

        if (fill_color_) {
            out << " fill=\""sv << *fill_color_ << "\""sv;
        }
        if (stroke_color_) {
            out << " stroke=\""sv << *stroke_color_ << "\""sv;
        }
        if (stroke_width_) {
            out << " stroke-width=\""sv << *stroke_width_ << "\""sv;
        }
        if (stroke_linecap_) {
            out << " stroke-linecap=\""sv << *stroke_linecap_ << "\""sv;
        }
        if (stroke_linejoin_) {
            out << " stroke-linejoin=\""sv << *stroke_linejoin_ << "\""sv;
        }
    }
};

struct Point {
    Point() = default;
    Point(double x, double y)
        : x(x)
        , y(y) {
    }
    double x = 0;
    double y = 0;
};

/*
 * Вспомогательная структура, хранящая контекст для вывода SVG-документа с отступами.
 * Хранит ссылку на поток вывода, текущее значение и шаг отступа при выводе элемента
 */
struct RenderContext {
    RenderContext(std::ostream& out)
        : out(out) {
    }

    RenderContext(std::ostream& out, int indent_step, int indent = 0)
        : out(out)
        , indent_step(indent_step)
        , indent(indent) {
    }

    RenderContext Indented() const {
        return {out, indent_step, indent + indent_step};
    }

    void RenderIndent() const {
        for (int i = 0; i < indent; ++i) {
            out.put(' ');
        }
    }

    std::ostream& out;
    int indent_step = 0;
    int indent = 0;
};

/*
 * Абстрактный базовый класс Object служит для унифицированного хранения
 * конкретных тегов SVG-документа
 * Реализует паттерн "Шаблонный метод" для вывода содержимого тега
 */
class Object {
public:
    void Render(const RenderContext& context) const;

    virtual ~Object() = default;

private:
    virtual void RenderObject(const RenderContext& context) const = 0;
};

/*
 * Класс Circle моделирует элемент <circle> для отображения круга
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/circle
 */
class Circle final : public Object, public PathProps<Circle> {
    Point center_;
    double radius_ = 1.0;
    void RenderObject(const RenderContext& context) const override;
public:
    Circle& SetCenter(Point center);
    Circle& SetRadius(double radius);
};

/*
 * Класс Polyline моделирует элемент <polyline> для отображения ломаных линий
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/polyline
 */
class Polyline final : public Object, public PathProps<Polyline> {
    std::list<Point> points_;
    void RenderObject(const RenderContext& context) const override;
public:
    // Добавляет очередную вершину к ломаной линии
    Polyline& AddPoint(Point point);
};

/*
 * Класс Text моделирует элемент <text> для отображения текста
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/text
 */
class Text final : public Object, public PathProps<Text> {
    Point pos_{0, 0};
    Point offset_{0, 0};
    uint32_t font_size_ = 1;
    std::string font_family_;
    std::string font_weight_;
    std::string data_;
    void RenderObject(const RenderContext& context) const override;
public:
    // Задаёт координаты опорной точки (атрибуты x и y)
    Text& SetPosition(Point pos);

    // Задаёт смещение относительно опорной точки (атрибуты dx, dy)
    Text& SetOffset(Point offset);

    // Задаёт размеры шрифта (атрибут font-size)
    Text& SetFontSize(uint32_t size);

    // Задаёт название шрифта (атрибут font-family)
    Text& SetFontFamily(std::string font_family);

    // Задаёт толщину шрифта (атрибут font-weight)
    Text& SetFontWeight(std::string font_weight);

    // Задаёт текстовое содержимое объекта (отображается внутри тега text)
    Text& SetData(std::string data);
};

class ObjectContainer {
protected:
    std::list<std::unique_ptr<Object>> objs_;
public:
    template<typename Obj> void Add(Obj obj) {
        objs_.emplace_back(std::make_unique<Obj>(std::move(obj)));
    }
    virtual void AddPtr(std::unique_ptr<Object>&& pobj) = 0;
    virtual ~ObjectContainer() {}
    ObjectContainer() = default;
    ObjectContainer(const ObjectContainer&) = default;
    ObjectContainer& operator=(const ObjectContainer&) = default;
    ObjectContainer(ObjectContainer&&) = default;
    ObjectContainer& operator=(ObjectContainer&&) = default;
};

class Drawable {
public:
    virtual void Draw(ObjectContainer & container) const = 0;
    virtual ~Drawable() {}
};

class Document : public ObjectContainer {
public:
    Document() = default;
    Document(const Document&) = default;
    Document& operator=(const Document&) = default;
    Document(Document&&) = default;
    Document& operator=(Document&&) = default;

    /*
     Метод Add добавляет в svg-документ любой объект-наследник svg::Object.
     Пример использования:
     Document doc;
     doc.Add(Circle().SetCenter({20, 30}).SetRadius(15));
    */

    // Добавляет в svg-документ объект-наследник svg::Object
    void AddPtr(std::unique_ptr<Object>&& pobj) override;

    // Выводит в ostream svg-представление документа
    void Render(std::ostream& out) const;
};

}  // namespace svg
