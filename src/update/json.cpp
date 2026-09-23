#include "json.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace minigit::update {

namespace {

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input), pos_(0) {}

    JsonValue parse() {
        skip_whitespace();
        if (pos_ >= input_.size()) return JsonValue();
        JsonValue val = parse_value();
        skip_whitespace();
        return val;
    }

private:
    std::string_view input_;
    size_t pos_{0};

    char peek() const {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    char get() {
        return pos_ < input_.size() ? input_[pos_++] : '\0';
    }

    void skip_whitespace() {
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos_++;
            } else {
                break;
            }
        }
    }

    JsonValue parse_value() {
        skip_whitespace();
        char c = peek();
        if (c == 'n') return parse_null();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == '"') return parse_string();
        if (c == '[') return parse_array();
        if (c == '{') return parse_object();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        return JsonValue();
    }

    JsonValue parse_null() {
        if (input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return JsonValue();
        }
        return JsonValue();
    }

    JsonValue parse_bool() {
        if (input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return JsonValue(true);
        }
        if (input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return JsonValue(false);
        }
        return JsonValue();
    }

    JsonValue parse_string() {
        if (get() != '"') return JsonValue();
        std::string result;
        while (pos_ < input_.size()) {
            char c = get();
            if (c == '"') {
                return JsonValue(result);
            }
            if (c == '\\') {
                if (pos_ >= input_.size()) break;
                char esc = get();
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 <= input_.size()) {
                            std::string hex_str(input_.substr(pos_, 4));
                            pos_ += 4;
                            char* endptr = nullptr;
                            long code = std::strtol(hex_str.c_str(), &endptr, 16);
                            if (endptr == hex_str.c_str() + 4) {
                                if (code <= 0x7F) {
                                    result += static_cast<char>(code);
                                } else if (code <= 0x7FF) {
                                    result += static_cast<char>(0xC0 | ((code >> 6) & 0x1F));
                                    result += static_cast<char>(0x80 | (code & 0x3F));
                                } else {
                                    result += static_cast<char>(0xE0 | ((code >> 12) & 0x0F));
                                    result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                    result += static_cast<char>(0x80 | (code & 0x3F));
                                }
                            }
                        }
                        break;
                    }
                    default: result += esc; break;
                }
            } else {
                result += c;
            }
        }
        return JsonValue(result);
    }

    JsonValue parse_number() {
        size_t start = pos_;
        if (peek() == '-') pos_++;
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
            pos_++;
        }
        if (peek() == '.') {
            pos_++;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
                pos_++;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            pos_++;
            if (peek() == '+' || peek() == '-') pos_++;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
                pos_++;
            }
        }
        std::string num_str(input_.substr(start, pos_ - start));
        char* endptr = nullptr;
        double val = std::strtod(num_str.c_str(), &endptr);
        return JsonValue(val);
    }

    JsonValue parse_array() {
        if (get() != '[') return JsonValue();
        std::vector<JsonValue> arr;
        skip_whitespace();
        if (peek() == ']') {
            get();
            return JsonValue(arr);
        }
        while (pos_ < input_.size()) {
            arr.push_back(parse_value());
            skip_whitespace();
            char c = peek();
            if (c == ']') {
                get();
                return JsonValue(arr);
            }
            if (c == ',') {
                get();
                skip_whitespace();
            } else {
                break;
            }
        }
        return JsonValue(arr);
    }

    JsonValue parse_object() {
        if (get() != '{') return JsonValue();
        std::unordered_map<std::string, JsonValue> obj;
        skip_whitespace();
        if (peek() == '}') {
            get();
            return JsonValue(obj);
        }
        while (pos_ < input_.size()) {
            skip_whitespace();
            if (peek() != '"') break;
            JsonValue key_val = parse_string();
            std::string key = key_val.as_string();
            skip_whitespace();
            if (peek() != ':') break;
            get(); // skip ':'
            skip_whitespace();
            JsonValue val = parse_value();
            obj[std::move(key)] = std::move(val);
            skip_whitespace();
            char c = peek();
            if (c == '}') {
                get();
                return JsonValue(obj);
            }
            if (c == ',') {
                get();
                skip_whitespace();
            } else {
                break;
            }
        }
        return JsonValue(obj);
    }
};

const JsonValue kNullValue;

} // namespace

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (type_ != JsonType::Object) return kNullValue;
    auto it = obj_val_.find(key);
    if (it != obj_val_.end()) return it->second;
    return kNullValue;
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (type_ != JsonType::Array || index >= arr_val_.size()) return kNullValue;
    return arr_val_[index];
}

JsonValue JsonValue::parse(std::string_view json_str) {
    JsonParser parser(json_str);
    return parser.parse();
}

} // namespace minigit::update
