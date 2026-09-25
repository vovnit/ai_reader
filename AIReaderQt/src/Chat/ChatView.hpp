#pragma once

#include "Common/Screen.hpp"
#include "Features/Chat/ChatFeature.hpp"

class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

/// A conversation about something in front of the reader: the thread, and
/// the question field under it.
class ChatView : public Screen {
public:
    /// What a conversation opens on.
    struct Seed {
        /// Sent to the model with the first question, as `ChatPrompt` builds it.
        std::string context;
        /// Shown in the empty thread: what this conversation is for.
        std::string hint;
        /// The books the model may search while answering.
        ReadingScope scope;
    };

    ChatView(Env& env, Navigator& navigator, const Seed& seed);

private:
    ChatFeature feature_;
    std::string hint_;
    QVBoxLayout* thread_;
    QScrollArea* scroller_;
    QLineEdit* question_;
    QPushButton* send_;

    void render();
};
