#include "DictionaryTool.hpp"

#include "ToolSchema.hpp"

namespace DictionaryTool {

const char* const toolName = "lookup_dictionary";

Json tool() {
    return ToolSchema::function(
        toolName,
        "Ищет слово в офлайн-словаре и возвращает найденные статьи.",
        "word",
        "Форма слова, которую нужно найти.");
}

std::string word(const std::string& arguments) {
    return ToolSchema::argument(arguments, "word", "");
}

}  // namespace DictionaryTool
