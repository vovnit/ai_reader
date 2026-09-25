#include "SearchTool.hpp"

#include "ToolSchema.hpp"

namespace SearchTool {

const char* const toolName = "search_book";

Json tool() {
    return ToolSchema::function(
        toolName,
        "Ищет слово или фразу в тексте книги, которую читает пользователь (и в других книгах той же серии), "
        "и возвращает отрывки, где она встречается, — только до места, до которого читатель дочитал.",
        "query",
        "Слово или короткая фраза, которую нужно найти в книге.");
}

std::string query(const std::string& arguments) {
    return ToolSchema::argument(arguments, "query", "");
}

std::string summary(const std::string& query, const std::vector<SearchHit>& hits, bool severalBooks) {
    if (hits.empty()) return "No passage read so far contains “" + query + "”.";
    std::string text = "Passages with “" + query + "”:\n";
    for (size_t i = 0; i < hits.size(); ++i) {
        const SearchHit& hit = hits[i];
        text += std::to_string(i + 1) + ". [";
        if (severalBooks) text += hit.bookTitle + ", ";
        text += "chapter " + std::to_string(hit.chapter + 1) + "] " + hit.excerpt + "\n";
    }
    return text;
}

}  // namespace SearchTool
