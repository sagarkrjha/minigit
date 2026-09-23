#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace minigit::update {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    JsonValue() : type_(JsonType::Null) {}
    explicit JsonValue(bool val) : type_(JsonType::Boolean), bool_val_(val) {}
    explicit JsonValue(double val) : type_(JsonType::Number), num_val_(val) {}
    explicit JsonValue(std::string val) : type_(JsonType::String), str_val_(std::move(val)) {}
    explicit JsonValue(std::vector<JsonValue> arr) : type_(JsonType::Array), arr_val_(std::move(arr)) {}
    explicit JsonValue(std::unordered_map<std::string, JsonValue> obj) : type_(JsonType::Object), obj_val_(std::move(obj)) {}

    JsonType type() const { return type_; }
    bool is_null() const { return type_ == JsonType::Null; }
    bool is_bool() const { return type_ == JsonType::Boolean; }
    bool is_number() const { return type_ == JsonType::Number; }
    bool is_string() const { return type_ == JsonType::String; }
    bool is_array() const { return type_ == JsonType::Array; }
    bool is_object() const { return type_ == JsonType::Object; }

    bool as_bool() const { return bool_val_; }
    double as_number() const { return num_val_; }
    const std::string& as_string() const { return str_val_; }
    const std::vector<JsonValue>& as_array() const { return arr_val_; }
    const std::unordered_map<std::string, JsonValue>& as_object() const { return obj_val_; }

    bool contains(const std::string& key) const {
        if (type_ != JsonType::Object) return false;
        return obj_val_.find(key) != obj_val_.end();
    }

    const JsonValue& operator[](const std::string& key) const;
    const JsonValue& operator[](size_t index) const;

    std::string get_string(const std::string& key, const std::string& def = "") const {
        const auto& v = (*this)[key];
        return v.is_string() ? v.as_string() : def;
    }

    double get_number(const std::string& key, double def = 0.0) const {
        const auto& v = (*this)[key];
        return v.is_number() ? v.as_number() : def;
    }

    bool get_bool(const std::string& key, bool def = false) const {
        const auto& v = (*this)[key];
        return v.is_bool() ? v.as_bool() : def;
    }

    static JsonValue parse(std::string_view json_str);

private:
    JsonType type_{JsonType::Null};
    bool bool_val_{false};
    double num_val_{0.0};
    std::string str_val_;
    std::vector<JsonValue> arr_val_;
    std::unordered_map<std::string, JsonValue> obj_val_;
};

} // namespace minigit::update
