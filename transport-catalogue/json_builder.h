#pragma once
#include "json.h"
#include <cassert>
#include <stack>
#include <string>
#include <optional>

/*
Код работы с обновлённым json::Builder не должен компилироваться в следующих ситуациях:
Непосредственно после Key вызван не Value, не StartDict и не StartArray.
После вызова Value, последовавшего за вызовом Key, вызван не Key и не EndDict.
За вызовом StartDict следует не Key и не EndDict.
За вызовом StartArray следует не Value, не StartDict, не StartArray и не EndArray.
После вызова StartArray и серии Value следует не Value, не StartDict, не StartArray и не EndArray.

json::Builder{}.StartDict().Build();  // правило 3
json::Builder{}.StartDict().Key("1"s).Value(1).Value(1);  // правило 2
json::Builder{}.StartDict().Key("1"s).Key(""s);  // правило 1
json::Builder{}.StartArray().Key("1"s);  // правило 4
json::Builder{}.StartArray().EndDict();  // правило 4
json::Builder{}.StartArray().Value(1).Value(2).EndDict();  // правило 5
*/

namespace json {

class Builder;
class DictItemContext;

class ArrayItemContext {
    Builder & builder_;
public:
    ArrayItemContext(Builder & builder);
    ArrayItemContext Value(json::Node);
    DictItemContext StartDict();
    ArrayItemContext StartArray();
    Builder &EndArray();
};

class KeyContext {
    Builder & builder_;
public:
    KeyContext(Builder & builder);
    DictItemContext Value(json::Node value);
    DictItemContext StartDict();
    ArrayItemContext StartArray();
};

class DictItemContext {
    Builder & builder_;
public:
    DictItemContext(Builder & builder);
    KeyContext Key(std::string key);
    Builder& EndDict();
};

class Builder {
    std::optional<json::Node> root_;
    std::stack<json::Node*> nodes_;
    std::stack<std::string> dict_key_;
    bool was_key_ = false;
    bool in_dict_ = false;
    bool open_for_new_values_ = true;

public:
    Builder() = default;
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    Builder(Builder&&) = delete;
    Builder& operator=(Builder&&) = delete;
    ~Builder();

    DictItemContext StartDict();
    Builder& EndDict();
    KeyContext Key(std::string value);
    Builder& Value(json::Node value);
    ArrayItemContext StartArray();
    Builder& EndArray();
    json::Node& Build();
};

} // namespace json
