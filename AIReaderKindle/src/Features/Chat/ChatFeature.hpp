#pragma once

#include "../../Domain/AI/ChatPrompt.hpp"
#include "../../Services/BookCorpus.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

/// A conversation about something in front of the reader: the page, a word
/// just explained, what the book says about a name. The context is sent
/// once, with the first question, so that follow-ups cost only the thread so
/// far. The model may open the dictionary and search the book while
/// answering.
class ChatFeature {
public:
    ChatFeature(Env& env, std::string context, ReadingScope scope);

    const std::vector<ChatTurn>& turns() const { return turns_; }
    bool isAnswering() const { return isAnswering_; }
    const std::string& error() const { return error_; }

    /// Sends a question; an empty one, or one asked while answering, is ignored.
    void send(const std::string& question);

    std::function<void()> onChange;

private:
    Env& env_;
    std::string context_;
    ReadingScope scope_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::vector<ChatTurn> turns_;
    bool isAnswering_ = false;
    std::string error_;
};
