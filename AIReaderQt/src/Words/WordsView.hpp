#pragma once

#include "Common/Screen.hpp"
#include "Features/Words/WordsFeature.hpp"

class QVBoxLayout;

/// Every word looked up so far, newest first, with the way to practise them
/// and to take them to Anki.
class WordsView : public Screen {
public:
    /// `bookId` narrows the list to one book; 0 shows every word met.
    WordsView(Env& env, Navigator& navigator, long long bookId);

private:
    Env& env_;
    WordsFeature feature_;
    QVBoxLayout* list_;

    void render();
    QWidget* row(const Lookup& lookup);
    void exportCards();
};
