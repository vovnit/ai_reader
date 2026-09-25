#pragma once

#include <string>
#include <vector>

/// Small string helpers. Everything here is UTF-8 aware where it matters.
namespace Text {

std::string trim(const std::string& text);
/// True when nothing but whitespace (non-breaking spaces included) and
/// illustration placeholders is left.
bool isBlank(const std::string& text);
std::string lower(const std::string& text);
bool startsWith(const std::string& text, const std::string& prefix);
bool endsWith(const std::string& text, const std::string& suffix);
bool contains(const std::string& text, const std::string& part);
std::vector<std::string> split(const std::string& text, char separator);
std::string join(const std::vector<std::string>& parts, const std::string& separator);
std::string replaceAll(std::string text, const std::string& from, const std::string& to);

}  // namespace Text
