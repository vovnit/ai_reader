#pragma once

#include "Support/Json.hpp"
#include "ChatMessage.hpp"
#include "WordExplanation.hpp"

#include <string>
#include <vector>

/// One turn of a conversation.
struct ChatTurn {
    bool isReader;
    std::string text;
};

/// Builds the conversation sent to the model. What it is about — the page on
/// screen, a word just explained — goes in once, with the first question.
namespace ChatPrompt {

extern const char* const system;

/// What the conversation starts from, as the model reads it.
std::string pageContext(const std::string& page);
std::string wordContext(const std::string& word, const std::string& sentence, const WordExplanation& explanation);
std::string xrayContext(const std::string& term, const std::string& answer);

std::vector<ChatMessage> messages(const std::string& context, const std::vector<ChatTurn>& turns);
/// The dictionary and the book search, both open to the model in a conversation.
Json tools();

}  // namespace ChatPrompt
