#pragma once

#include "Support/Json.hpp"

#include <string>
#include <vector>

/// The tool that lets the model look beyond the book and the dictionary:
/// `search_web` finds pages about a name, a place, an event or an
/// expression, and hands the model a few of them with their text.
namespace WebSearchTool {

extern const char* const toolName;

/// How many pages a call returns to the model.
constexpr int resultLimit = 5;
/// How much of one page's text the model gets.
constexpr size_t textLimit = 600;

/// One page found.
struct WebHit {
    std::string title;
    std::string url;
    std::string text;
};

Json tool();
std::string query(const std::string& arguments);

/// The pages in a search endpoint's answer, whatever the provider: the
/// first list of objects found, read by the usual field names — title or
/// name, url or link, text, snippet or description.
std::vector<WebHit> hits(const Json& output);

/// The pages as the model reads them: numbered, with their address and
/// text. When none can be read out of `output`, the model gets the
/// answer as it came, cut to a readable length, rather than nothing.
std::string summary(const std::string& query, const Json& output);

}  // namespace WebSearchTool
