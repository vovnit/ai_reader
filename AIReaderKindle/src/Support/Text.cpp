#include "Text.hpp"

#include <glib.h>

namespace Text {

std::string trim(const std::string& text) {
    const char* spaces = " \t\r\n";
    auto start = text.find_first_not_of(spaces);
    if (start == std::string::npos) return "";
    auto end = text.find_last_not_of(spaces);
    return text.substr(start, end - start + 1);
}

bool isBlank(const std::string& text) {
    const char* p = text.c_str();
    const char* end = p + text.size();
    for (; p < end; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (!g_unichar_isspace(c) && c != 0xFFFC) return false;
    }
    return true;
}

std::string lower(const std::string& text) {
    gchar* lowered = g_utf8_strdown(text.c_str(), text.size());
    std::string result = lowered ? lowered : "";
    g_free(lowered);
    return result;
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == separator) {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

std::string join(const std::vector<std::string>& parts, const std::string& separator) {
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) result += separator;
        result += parts[i];
    }
    return result;
}

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    if (from.empty()) return text;
    size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
    return text;
}

}  // namespace Text
