#include "WebSearchTool.hpp"

#include "../../Support/Text.hpp"
#include "ToolSchema.hpp"

#include <glib.h>

namespace WebSearchTool {

const char* const toolName = "search_web";

Json tool() {
    return ToolSchema::function(
        toolName,
        "Ищет в интернете и возвращает несколько найденных страниц с фрагментами их текста. "
        "Используй, когда ни словарь, ни книга не помогают: имя, место, событие, реалия, "
        "название, сленг или выражение, которых нет в словаре. Не ищи то, что есть в словаре.",
        "query",
        "Что искать: короткий запрос, как в поисковой строке.");
}

std::string query(const std::string& arguments) {
    return ToolSchema::argument(arguments, "query", "");
}

namespace {

std::string field(const Json& item, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const Json& value = item.at(name);
        if (value.isString() && !value.string().empty()) return value.string();
    }
    return "";
}

/// The first array of objects in `value`, searching breadth-first so
/// `{"results": [...]}` is found before anything nested deeper.
const Json* firstList(const Json& value) {
    if (value.isArray()) return value.size() > 0 && value.at(0).isObject() ? &value : nullptr;
    if (!value.isObject()) return nullptr;
    for (const auto& member : value.members()) {
        if (member.second.isArray() && member.second.size() > 0 && member.second.at(0).isObject()) return &member.second;
    }
    for (const auto& member : value.members()) {
        if (const Json* found = firstList(member.second)) return found;
    }
    return nullptr;
}

/// Cut on a character boundary, with an ellipsis when something was cut.
std::string shorten(const std::string& text, size_t limit) {
    if (text.size() <= limit) return text;
    const char* end = g_utf8_prev_char(text.c_str() + limit + 1);
    return text.substr(0, static_cast<size_t>(end - text.c_str())) + "…";
}

}  // namespace

std::vector<WebHit> hits(const Json& output) {
    std::vector<WebHit> found;
    const Json* list = firstList(output);
    if (!list) return found;
    for (const Json& item : list->items()) {
        WebHit hit{
            field(item, {"title", "name"}),
            field(item, {"url", "link", "href"}),
            field(item, {"text", "snippet", "description", "content", "summary"}),
        };
        if (hit.title.empty() && hit.url.empty() && hit.text.empty()) continue;
        hit.text = shorten(Text::trim(Text::replaceAll(hit.text, "\n", " ")), textLimit);
        found.push_back(std::move(hit));
        if (found.size() == resultLimit) break;
    }
    return found;
}

std::string summary(const std::string& query, const Json& output) {
    std::vector<WebHit> found = hits(output);
    if (found.empty()) {
        if (output.isNull() || (output.isArray() && output.size() == 0)) return "Nothing was found on the web for “" + query + "”.";
        return "The web search for “" + query + "” answered:\n" + shorten(output.dump(), 3000);
    }
    std::string text = "Pages found for “" + query + "”:\n";
    for (size_t i = 0; i < found.size(); ++i) {
        const WebHit& hit = found[i];
        text += std::to_string(i + 1) + ". " + (hit.title.empty() ? "(untitled)" : hit.title);
        if (!hit.url.empty()) text += " — " + hit.url;
        text += "\n";
        if (!hit.text.empty()) text += "   " + hit.text + "\n";
    }
    return text;
}

}  // namespace WebSearchTool
