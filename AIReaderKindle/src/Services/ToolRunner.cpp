#include "ToolRunner.hpp"

#include "../Domain/AI/ContextTool.hpp"
#include "../Domain/AI/DictionaryTool.hpp"
#include "../Domain/AI/MockAI.hpp"
#include "../Domain/AI/SearchTool.hpp"
#include "../Domain/AI/WebSearchTool.hpp"
#include "ChatApi.hpp"
#include "DictionaryDatabase.hpp"
#include "WebSearch.hpp"

namespace ToolRunner {

namespace {

/// `tools` with the web added when it can be answered.
Json offered(const Json& tools, const AiSettings& settings, const Tools& available) {
    if (!settings.usesMock() && !available.web.isConfigured()) return tools;
    Json all = Json::array();
    for (const auto& tool : tools.items()) all.push(tool);
    all.push(WebSearchTool::tool());
    return all;
}

/// `window` is how much of the book around the passage the model has read
/// so far, widened by each `expand_context` call.
std::string answer(const ChatMessage::ToolCall& call, const AiSettings& settings, const Tools& available,
                   std::optional<BookPassage>& window) {
    if (call.name == DictionaryTool::toolName) {
        std::string word = DictionaryTool::word(call.arguments);
        return DictionaryDatabase::shared().lookup(word, available.packs).summary();
    }
    if (call.name == SearchTool::toolName) {
        std::string query = SearchTool::query(call.arguments);
        const ReadingScope& scope = available.scope;
        if (!scope.corpus || query.empty()) return SearchTool::summary(query, {}, false);
        auto hits = scope.corpus->search(query, SearchTool::passageLimit, scope.upTo);
        return SearchTool::summary(query, hits, scope.corpus->severalBooks());
    }
    if (call.name == ContextTool::toolName) {
        auto direction = ContextTool::direction(call.arguments);
        if (!direction) return ContextTool::unknownDirection;
        if (!window || !available.scope.corpus) return ContextTool::nothingAround;
        std::string chapter = available.scope.corpus->chapterText(window->bookId, window->chapter);
        if (chapter.empty()) return ContextTool::nothingAround;
        return ContextTool::read(*direction, chapter, *window);
    }
    if (call.name == WebSearchTool::toolName) {
        std::string query = WebSearchTool::query(call.arguments);
        if (query.empty()) return WebSearchTool::summary(query, Json());
        // A failed search is told to the model, which can still answer
        // from what it has, rather than failing the whole conversation.
        try {
            // Pages in the book's language: a French name wants the French page.
            const auto& books = available.scope.corpus ? available.scope.corpus->books() : std::vector<Book>{};
            std::string language = books.empty() ? "" : books.front().language;
            Json output = settings.usesMock() ? MockAI::webSearch(query) : WebSearch::search(available.web, query, language);
            return WebSearchTool::summary(query, output);
        } catch (const std::exception& failure) {
            return std::string("The web search failed: ") + failure.what();
        }
    }
    return "There is no tool called “" + call.name + "”.";
}

}  // namespace

ChatMessage converse(
    const AiSettings& settings,
    std::vector<ChatMessage>& messages,
    const Json& tools,
    bool jsonMode,
    const Tools& available)
{
    Json all = offered(tools, settings, available);
    std::optional<BookPassage> window = available.scope.passage;
    for (int round = 0; round <= budget; ++round) {
        ChatMessage reply = ChatApi::chat(settings, messages, all, jsonMode);
        if (reply.toolCalls.empty()) return reply;
        messages.push_back(reply);
        for (const auto& call : reply.toolCalls) {
            messages.push_back(ChatMessage::toolResult(answer(call, settings, available, window), call.id));
        }
    }
    throw ChatApi::Error("The model kept searching without answering.");
}

}  // namespace ToolRunner
