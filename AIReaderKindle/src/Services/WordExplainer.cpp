#include "WordExplainer.hpp"

#include "../Domain/AI/ExplanationPrompt.hpp"
#include "ChatApi.hpp"
#include "DictionaryDatabase.hpp"

namespace WordExplainer {

WordExplanation explain(
    const std::string& word,
    const std::string& sentence,
    const AiSettings& settings,
    const ToolRunner::Tools& tools)
{
    DictionaryLookup lookup = DictionaryDatabase::shared().lookup(word, tools.packs);
    std::vector<ChatMessage> messages = {
        ChatMessage::system(ExplanationPrompt::system),
        ChatMessage::user(ExplanationPrompt::question(word, sentence, lookup)),
    };
    ChatMessage reply = ToolRunner::converse(settings, messages, ExplanationPrompt::tools(), true, tools);
    auto explanation = WordExplanation::decode(reply.content.value_or(""));
    if (!explanation) throw ChatApi::Error("The model's answer could not be read.");
    return *explanation;
}

}  // namespace WordExplainer
