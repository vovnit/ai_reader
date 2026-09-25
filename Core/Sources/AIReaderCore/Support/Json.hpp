#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

/// A JSON value: enough to build request bodies and read replies. Objects keep
/// their members in insertion order so a serialized request reads the way it
/// was written.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Members = std::vector<std::pair<std::string, Json>>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool value);
    Json(int value);
    Json(long long value);
    Json(double value);
    Json(const char* value);
    Json(std::string value);
    Json(std::vector<Json> items);

    static Json object();
    static Json array();

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isString() const { return type_ == Type::String; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool boolean(bool fallback = false) const;
    double number(double fallback = 0) const;
    const std::string& string() const;

    /// A missing key or index reads as null rather than failing.
    const Json& at(const std::string& key) const;
    const Json& at(size_t index) const;
    bool has(const std::string& key) const;
    size_t size() const;
    const std::vector<Json>& items() const { return items_; }
    const Members& members() const { return members_; }

    Json& set(const std::string& key, Json value);
    Json& push(Json value);

    std::string dump() const;
    static std::optional<Json> parse(const std::string& text);

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0;
    std::string string_;
    std::vector<Json> items_;
    Members members_;
};
