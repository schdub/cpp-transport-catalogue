#include "json_builder.h"
#include "json.h"
#include <cassert>
#include <stack>
#include <string>
#include <sstream>

namespace json {

#define LOGIC_ERROR_INTERNAL(statement, function, message) \
if (!!(statement)) {                     \
    std::stringstream ss;                \
    ss << function << "(): " << message; \
    throw std::logic_error(ss.str());    \
}
#define LOGIC_ERROR(statement, message) LOGIC_ERROR_INTERNAL(statement, __FUNCTION__, message)

// ///////////////////////////////////////////////////////////////////// //

ArrayItemContext::ArrayItemContext(Builder & builder)
    : builder_(builder)
{}
ArrayItemContext ArrayItemContext::Value(json::Node value) {
    builder_.Value(std::move(value));
    return {builder_};
}
DictItemContext ArrayItemContext::StartDict() {
    builder_.StartDict();
    return {builder_};
}
ArrayItemContext ArrayItemContext::StartArray() {
    builder_.StartArray();
    return {builder_};
}
Builder& ArrayItemContext::EndArray() {
    builder_.EndArray();
    return builder_;
}

KeyContext::KeyContext(Builder & builder)
    : builder_(builder)
{}
DictItemContext KeyContext::Value(json::Node value) {
    builder_.Value(std::move(value));
    return {builder_};
}
DictItemContext KeyContext::StartDict() {
    builder_.StartDict();
    return {builder_};
}
ArrayItemContext KeyContext::StartArray() {
    builder_.StartArray();
    return {builder_};
}

DictItemContext::DictItemContext(Builder & builder)
    : builder_(builder)
{}
KeyContext DictItemContext::Key(std::string key) {
    builder_.Key(std::move(key));
    return {builder_};
}
Builder& DictItemContext::EndDict() {
    builder_.EndDict();
    return builder_;
}

// ///////////////////////////////////////////////////////////////////// //

Builder::~Builder() {
    while (!nodes_.empty()) {
        delete nodes_.top();
        nodes_.pop();
    }
}

// StartDict(). Начинает определение сложного значения-словаря. Вызывается в
// тех же контекстах, что и Value. Следующим вызовом обязательно должен быть
// Key или EndDict.

DictItemContext Builder::StartDict() {
    LOGIC_ERROR(!open_for_new_values_, "!open_for_new_values_");
    LOGIC_ERROR(root_.has_value(), "already constucted");
    LOGIC_ERROR(in_dict_, "in_dict_");
    in_dict_ = true;
    was_key_ = false;
    nodes_.push(new json::Node(json::Dict{}));
    return DictItemContext{*this};
}

// EndDict(). Завершает определение сложного значения-словаря. Последним
// незавершённым вызовом Start* должен быть StartDict.

Builder& Builder::EndDict() {
    open_for_new_values_ = true;
    LOGIC_ERROR(nodes_.empty() || !nodes_.top()->IsMap(), "failed");
    in_dict_ = false;
    auto *pDict = nodes_.top();
    nodes_.pop();
    auto temp(*(pDict));
    delete pDict;

    if (nodes_.empty()) {
        LOGIC_ERROR(root_.has_value(), "already conttructed root object");
        root_ = std::move(temp);
    } else {
        if (nodes_.top()->IsMap()) {
            in_dict_ = true;
            if (dict_key_.empty()) {
                LOGIC_ERROR(root_.has_value(), "already constructed root object");
                root_ = std::move(temp);
            } else {
                auto & ref_map = nodes_.top()->AsMap();
                auto key = std::move(dict_key_.top());
                dict_key_.pop();
                ref_map.emplace(std::make_pair(std::move(key), std::move(temp)));
            }
        } else if (nodes_.top()->IsArray()) {
            auto & ref_arr = nodes_.top()->AsArray();
            ref_arr.emplace_back(std::move(temp));
        }
    }
    return *this;
}

// Key(std::string). При определении словаря задаёт строковое значение ключа для
// очередной пары ключ-значение. Следующий вызов метода обязательно должен
// задавать соответствующее этому ключу значение с помощью метода Value или
// начинать его определение с помощью StartDict или StartArray.

KeyContext Builder::Key(std::string value) {
    open_for_new_values_ = true;
    in_dict_ = false;
    LOGIC_ERROR(was_key_, "key after key");
    was_key_ = true;
    bool is_dict = false;
    if (!nodes_.empty()) {
        is_dict  = nodes_.top()->IsMap();
    }
    LOGIC_ERROR(!is_dict, "we're not in dictionary");
    dict_key_.push(std::move(value));
    return {*this};
}

// Value(Node::Value). Задаёт значение, соответствующее ключу при определении
// словаря, очередной элемент массива или, если вызвать сразу после конструктора
// json::Builder, всё содержимое конструируемого JSON-объекта. Может принимать
// как простой объект — число или строку — так и целый массив или словарь. Здесь
// Node::Value — это синоним для базового класса Node, шаблона variant с набором
// возможных типов-значений. Смотрите заготовку кода.

Builder& Builder::Value(json::Node value) {
    LOGIC_ERROR(!open_for_new_values_, "!open_for_new_values_");
    LOGIC_ERROR(in_dict_, "in_dict_");
    LOGIC_ERROR(root_.has_value(), "alredy constructed");
    open_for_new_values_ = true;
    bool is_dict = false;
    bool is_array = false;
    if (!nodes_.empty()) {
        is_dict  = nodes_.top()->IsMap();
        is_array = nodes_.top()->IsArray();
    }
    if (is_dict) {
        LOGIC_ERROR(!was_key_, "key for map not specified");
        was_key_ = false;
        auto & ref_dict = nodes_.top()->AsMap();
        auto key = dict_key_.top();
        dict_key_.pop();
        ref_dict.emplace(std::make_pair(std::move(key), std::move(value)));
    } else if (is_array) {
        auto & ref_array = nodes_.top()->AsArray();
        ref_array.emplace_back(std::move(value));
    } else {
        root_ = std::move(value);
    }
    return *this;
}

// StartArray(). Начинает определение сложного значения-массива. Вызывается в тех
// же контекстах, что и Value. Следующим вызовом обязательно должен быть EndArray
// или любой, задающий новое значение: Value, StartDict или StartArray.

ArrayItemContext Builder::StartArray() {
    LOGIC_ERROR(!open_for_new_values_, "!open_for_new_values_");
    LOGIC_ERROR(in_dict_, "in_dict_");
    LOGIC_ERROR(root_.has_value(), "alredy constructed");
    was_key_ = false;
    nodes_.push(new json::Node(json::Array{}));
    return {*this};
}

// EndArray(). Завершает определение сложного значения-массива. Последним
// незавершённым вызовом Start* должен быть StartArray.

Builder& Builder::EndArray() {
    open_for_new_values_ = true;
    LOGIC_ERROR(nodes_.empty() || !nodes_.top()->IsArray(), "failed");
    auto * pArray = nodes_.top();
    nodes_.pop();
    auto temp(*pArray);
    delete pArray;

    if (!nodes_.empty()) {
        if (nodes_.top()->IsMap()) {
            LOGIC_ERROR(dict_key_.empty(), "key not specified");
            auto & ref_map = nodes_.top()->AsMap();
            auto key = dict_key_.top();
            dict_key_.pop();
            ref_map.emplace(std::make_pair(std::move(key), std::move(temp)));
        } else if (nodes_.top()->IsArray()) {
            auto & ref_arr = nodes_.top()->AsArray();
            ref_arr.emplace_back(std::move(temp));
        }
    }
    if (nodes_.empty()) {
        root_ = std::move(temp);
    }
    return *this;
}

// Build(). Возвращает объект json::Node, содержащий JSON, описанный предыдущими вызовами
// методов. К этому моменту для каждого Start* должен быть вызван соответствующий End*.
// При этом сам объект должен быть определён, то есть вызов json::Builder{}.Build()
// недопустим.

json::Node& Builder::Build() {
    LOGIC_ERROR(!root_.has_value() || !nodes_.empty(), "!root_.has_value() || !nodes_.empty()");
    return root_.value();
}

} // namespace json
