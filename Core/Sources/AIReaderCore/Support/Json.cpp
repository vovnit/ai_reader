#include "Json.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

Json::Json(bool value) : type_(Type::Bool), bool_(value) {}
Json::Json(int value) : type_(Type::Number), number_(value) {}
Json::Json(long long value) : type_(Type::Number), number_(static_cast<double>(value)) {}
Json::Json(double value) : type_(Type::Number), number_(value) {}
Json::Json(const char* value) : type_(Type::String), string_(value) {}
Json::Json(std::string value) : type_(Type::String), string_(std::move(value)) {}
Json::Json(std::vector<Json> items) : type_(Type::Array), items_(std::move(items)) {}

Json Json::object() {
    Json json;
    json.type_ = Type::Object;
    return json;
}

Json Json::array() {
    Json json;
    json.type_ = Type::Array;
    return json;
}

bool Json::boolean(bool fallback) const { return type_ == Type::Bool ? bool_ : fallback; }
double Json::number(double fallback) const { return type_ == Type::Number ? number_ : fallback; }

const std::string& Json::string() const {
    static const std::string empty;
    return type_ == Type::String ? string_ : empty;
}

const Json& Json::at(const std::string& key) const {
    static const Json null;
    for (const auto& member : members_) {
        if (member.first == key) return member.second;
    }
    return null;
}

const Json& Json::at(size_t index) const {
    static const Json null;
    return index < items_.size() ? items_[index] : null;
}

bool Json::has(const std::string& key) const {
    for (const auto& member : members_) {
        if (member.first == key) return true;
    }
    return false;
}

size_t Json::size() const {
    return type_ == Type::Array ? items_.size() : type_ == Type::Object ? members_.size() : 0;
}

Json& Json::set(const std::string& key, Json value) {
    if (type_ != Type::Object) *this = object();
    for (auto& member : members_) {
        if (member.first == key) {
            member.second = std::move(value);
            return *this;
        }
    }
    members_.emplace_back(key, std::move(value));
    return *this;
}

Json& Json::push(Json value) {
    if (type_ != Type::Array) *this = array();
    items_.push_back(std::move(value));
    return *this;
}

// MARK: - Serializing

static void dumpString(const std::string& text, std::string& out) {
    out += '"';
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                out += buffer;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
}

static void dumpValue(const Json& json, std::string& out) {
    switch (json.type()) {
    case Json::Type::Null: out += "null"; break;
    case Json::Type::Bool: out += json.boolean() ? "true" : "false"; break;
    case Json::Type::Number: {
        double value = json.number();
        char buffer[32];
        if (std::floor(value) == value && std::fabs(value) < 1e15) {
            std::snprintf(buffer, sizeof buffer, "%lld", static_cast<long long>(value));
        } else {
            // The shortest form that reads back as the same number.
            std::snprintf(buffer, sizeof buffer, "%.15g", value);
            if (std::strtod(buffer, nullptr) != value) std::snprintf(buffer, sizeof buffer, "%.17g", value);
        }
        out += buffer;
        break;
    }
    case Json::Type::String: dumpString(json.string(), out); break;
    case Json::Type::Array:
        out += '[';
        for (size_t i = 0; i < json.items().size(); ++i) {
            if (i) out += ',';
            dumpValue(json.items()[i], out);
        }
        out += ']';
        break;
    case Json::Type::Object:
        out += '{';
        for (size_t i = 0; i < json.members().size(); ++i) {
            if (i) out += ',';
            dumpString(json.members()[i].first, out);
            out += ':';
            dumpValue(json.members()[i].second, out);
        }
        out += '}';
        break;
    }
}

std::string Json::dump() const {
    std::string out;
    dumpValue(*this, out);
    return out;
}
