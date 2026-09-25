#pragma once

#include "../Domain/AI/ChatMessage.hpp"
#include "../Domain/Dictionary/DictionaryPack.hpp"
#include "Support/Json.hpp"
#include "BookCorpus.hpp"
#include "Settings.hpp"

#include <memory>
#include <optional>
#include <vector>

/// Carries a conversation with the model, answering the tools it calls —
/// the dictionary, the book search and the web — until it answers in
/// words. Runs on a worker thread; throws `ChatApi::Error` with a message
/// fit to show.
namespace ToolRunner {

/// What the model may reach for. A missing corpus or empty pack list simply
/// answers that tool with nothing. The web is offered only when it is set
/// up, since a search there may be paid for — and always under the mock,
/// which answers it itself.
struct Tools {
    ReadingScope scope;
    std::vector<DictionaryPack> packs;
    WebSearchSettings web;
};

/// How many rounds of tool calls the model gets before it must answer.
constexpr int budget = 3;

ChatMessage converse(
    const AiSettings& settings,
    std::vector<ChatMessage>& messages,
    const Json& tools,
    bool jsonMode,
    const Tools& available);

}  // namespace ToolRunner
