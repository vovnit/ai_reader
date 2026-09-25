#pragma once

#include "../../Services/BookCorpus.hpp"
#include "../../Services/Env.hpp"

#include <string>

class Navigator;

/// A conversation screen: the question field, the thread, the keyboard.
namespace ChatView {

/// What a conversation opens on.
struct Seed {
    /// Sent to the model with the first question, as `ChatPrompt` builds it.
    std::string context;
    /// Shown in the empty thread: what this conversation is for.
    std::string hint;
    /// The books the model may search while answering.
    ReadingScope scope;
};

void open(Env& env, Navigator& navigator, const Seed& seed);

}  // namespace ChatView
