#include "json.h"
#include "domain.h"
#include <stack>

using namespace std;
using namespace domain;

namespace json {

namespace {

using Number = std::variant<int, double>;

Number LoadNumber(std::istream& input) {
    using namespace std::literals;

    std::string parsed_num;

    // Считывает в parsed_num очередной символ из input
    auto read_char = [&parsed_num, &input] {
        parsed_num += static_cast<char>(input.get());
        if (!input) {
            throw ParsingError("Failed to read number from stream"s);
        }
    };

    // Считывает одну или более цифр в parsed_num из input
    auto read_digits = [&input, read_char] {
        if (!std::isdigit(input.peek())) {
            throw ParsingError("A digit is expected"s);
        }
        while (std::isdigit(input.peek())) {
            read_char();
        }
    };

    if (input.peek() == '-') {
        read_char();
    }
    // Парсим целую часть числа
    if (input.peek() == '0') {
        read_char();
        // После 0 в JSON не могут идти другие цифры
    } else {
        read_digits();
    }

    bool is_int = true;
    // Парсим дробную часть числа
    if (input.peek() == '.') {
        read_char();
        read_digits();
        is_int = false;
    }

    // Парсим экспоненциальную часть числа
    if (int ch = input.peek(); ch == 'e' || ch == 'E') {
        read_char();
        if (ch = input.peek(); ch == '+' || ch == '-') {
            read_char();
        }
        read_digits();
        is_int = false;
    }

    try {
        if (is_int) {
            // Сначала пробуем преобразовать строку в int
            try {
                return std::stoi(parsed_num);
            } catch (...) {
                // В случае неудачи, например, при переполнении,
                // код ниже попробует преобразовать строку в double
            }
        }
        return std::stod(parsed_num);
    } catch (...) {
        throw ParsingError("Failed to convert "s + parsed_num + " to number"s);
    }
}

// Считывает содержимое строкового литерала JSON-документа
// Функцию следует использовать после считывания открывающего символа ":
std::string LoadString(std::istream& input) {
    using namespace std::literals;

    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            // Поток закончился до того, как встретили закрывающую кавычку?
            throw ParsingError("String parsing error");
        }
        const char ch = *it;
        if (ch == '"') {
            // Встретили закрывающую кавычку
            ++it;
            break;
        } else if (ch == '\\') {
            // Встретили начало escape-последовательности
            ++it;
            if (it == end) {
                // Поток завершился сразу после символа обратной косой черты
                throw ParsingError("String parsing error");
            }
            const char escaped_char = *(it);
            // Обрабатываем одну из последовательностей: \\, \n, \t, \r, \"
            switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                default:
                    // Встретили неизвестную escape-последовательность
                    throw ParsingError("Unrecognized escape sequence \\"s + escaped_char);
            }
        } else if (ch == '\n' || ch == '\r') {
            // Строковый литерал внутри- JSON не может прерываться символами \r или \n
            throw ParsingError("Unexpected end of line"s);
        } else {
            // Просто считываем очередной символ и помещаем его в результирующую строку
            s.push_back(ch);
        }
        ++it;
    }

    return s;
}

Node LoadNode(istream& input);

Node LoadArray(istream& input) {
    Array result;
    char c = 0;
    for (; input >> c && c != ']';) {
        if (c != ',') {
            input.putback(c);
        }
        result.push_back(LoadNode(input));
    }
    if (c != ']') {
        throw ParsingError("ParseArray: Expected symbol ]"s);
    }
    return Node(move(result));
}

Node LoadSpecial(istream& input) {
    char arr[5];
    int i = 0;

    if (input >> arr[i++] &&
        input >> arr[i++] &&
        input >> arr[i++] &&
        input >> arr[i++]) {
        if (arr[0] == 'n' &&
            arr[1] == 'u' &&
            arr[2] == 'l' &&
            arr[3] == 'l') {
            return Node{};
        }
        if (arr[0] == 'N' &&
            arr[1] == 'U' &&
            arr[2] == 'L' &&
            arr[3] == 'L') {
            return Node{};
        }
        if (arr[0] == 't' &&
            arr[1] == 'r' &&
            arr[2] == 'u' &&
            arr[3] == 'e') {
            return Node(true);
        }
        if (arr[0] == 'f' &&
            arr[1] == 'a' &&
            arr[2] == 'l' &&
            arr[3] == 's') {
            if (input >> arr[i++]) {
                return Node(false);
            }
        }
    }
    throw ParsingError("can't load special value");
    return Node{};
}

Node LoadDict(istream& input) {
    Dict result;
    char c = 0;
    for (; input >> c && c != '}';) {
        if (c == ',') {
            input >> c;
        }
        std::string key = LoadString(input);
        input >> c;
        result.insert({move(key), LoadNode(input)});
    }
    if (c != '}') {
        throw ParsingError("expected '{'");
    }
    return Node(move(result));
}

Node LoadNode(istream& input) {
    // read whitespaces before something useful
    char c;
    while ((input >> c) && ::isspace(c));

    if (c == '[') {
        return LoadArray(input);
    } else if (c == '{') {
        return LoadDict(input);
    } else if (c == '"') {
        return LoadString(input);
    } else if (c == 'N' ||
               c == 'n' ||
               c == 'f' ||
               c == 't') {
        input.putback(c);
        return LoadSpecial(input);
    } else {
        input.putback(c);
        auto number = LoadNumber(input);
        if (std::holds_alternative<int>(number)) {
            return Node(std::get<int>(number));
        } else {
            return Node(std::get<double>(number));
        }
    }
}

}  // namespace

Node::Node(nullptr_t)
{}

Node::Node(Array array)
    : data_(move(array)) {
}

Node::Node(Dict map)
    : data_(move(map)) {
}

Node::Node(int value)
    : data_(value) {
}

Node::Node(string value)
    : data_(move(value)) {
}

Node::Node(double value)
    : data_(move(value)) {
}

Node::Node(bool value)
    : data_(move(value)) {
}

const Array& Node::AsArray() const {
    if (!IsArray()) throw logic_error("not array");
    return std::get<Array>(data_);
}

const Dict& Node::AsMap() const {
    if (!IsMap()) throw logic_error("not map");
    return std::get<Dict>(data_);
}

int Node::AsInt() const {
    if (!IsInt()) throw logic_error("not int");
    return std::get<int>(data_);
}

const string& Node::AsString() const {
    if (!IsString()) throw logic_error("not string");
    return std::get<std::string>(data_);
}

double Node::AsDouble() const {
    if (IsPureDouble()) {
        return std::get<double>(data_);
    }
    if (!IsInt()) throw logic_error("not int");
    return std::get<int>(data_);
}

bool Node::AsBool() const {
    if (!IsBool()) throw logic_error("not bool");
    return std::get<bool>(data_);
}

bool Node::IsInt() const {
    return std::holds_alternative<int>(data_);
}
// Возвращает true, если в Node хранится int либо double.
bool Node::IsDouble() const {
    return IsInt() || IsPureDouble();
}
// Возвращает true, если в Node хранится double.
bool Node::IsPureDouble() const {
    return std::holds_alternative<double>(data_);
}
bool Node::IsBool() const {
    return std::holds_alternative<bool>(data_);
}
bool Node::IsString() const {
    return std::holds_alternative<std::string>(data_);
}
bool Node::IsNull() const {
    return (data_.index() == 0);
}
bool Node::IsArray() const {
    return std::holds_alternative<Array>(data_);
}
bool Node::IsMap() const {
    return std::holds_alternative<Dict>(data_);
}

bool Node::operator==(const Node & other) const {
    if (this->IsNull() && other.IsNull()) {
        return true;
    }

    if (this->data_.index() == other.data_.index()){
        return (this->data_ == other.data_);
    }

    return false;
}

bool Node::operator!=(const Node & other) const {
    return !(*this == other);
}

// /////////////////////////////////////////// //

Document::Document(Node root)
    : root_(move(root)) {
}

const Node& Document::GetRoot() const {
    return root_;
}

bool Document::operator==(const Document & other) const {
    return this->GetRoot() == other.GetRoot();
}

bool Document::operator!=(const Document & other) const {
    return !(*this == other);
}

Document Load(istream& input) {
    return Document{LoadNode(input)};
}

void PrintNode(const Node& node, std::ostream& out);

template <typename Value>
void PrintValue(const Value& value, std::ostream& out) {
    out << value;
}

void PrintValue(std::nullptr_t, std::ostream& out) {
    out << "null"sv;
}

void PrintValue(bool value, std::ostream& out) {
    out << (value ? "true" : "false");
}

void PrintValue(const std::string & value, std::ostream& out) {
    auto str{value};
    // \n, \r, \", \t,
    // "\r\n\t\"\\"
    // \"\\r\\n\t\\\"\\\\\"
    replace(str, "\\"s, "\\\\"s);
    replace(str, "\""s, "\\\""s);
    replace(str, "\r"s, "\\r"s);
    replace(str, "\n"s, "\\n"s);
    //replace(str, "\t"s, "\\t"s);
    out << "\"" << str << "\"";
}

void PrintValue(const Array & arr, std::ostream & out) {
    out << "[";
    if (arr.size() > 0) {
        for (auto index = arr.begin();;) {
            PrintNode(*index, out);
            if (++index == arr.end()) break;
            out << ",";
        }
    }
    out << "]";
}

void PrintValue(const Dict & dict, std::ostream & out) {
    out << "{";
    if (dict.size() > 0) {
        for (auto index = dict.begin();;) {
            const auto & k = index->first;
            const auto & v = index->second;
            PrintValue(k, out);
            out << ": ";
            PrintNode(v, out);
            if (++index == dict.end()) break;
            out << ", ";
        }
    }
    out << "}";
}

void PrintNode(const Node& node, std::ostream& out) {
    std::visit(
        [&out](const auto& value){ PrintValue(value, out); },
        node.GetValue());
}

void Print(const Document& doc, std::ostream& output) {
    PrintNode(doc.GetRoot(), output);
}

}  // namespace json
