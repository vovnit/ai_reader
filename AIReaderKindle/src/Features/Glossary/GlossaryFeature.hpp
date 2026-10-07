#pragma once

#include "../../Domain/Books/Book.hpp"
#include "../../Domain/Reading/BookWords.hpp"
#include "../../Services/Env.hpp"

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/// A book's offline glossary: count the book's words, then ask the model
/// about those the glossary lacks, a batch at a time, appending each answer
/// as it arrives — so a stopped or failed run keeps what it paid for, and the
/// next asks only about the rest. When a run ends, the word list becomes the
/// book's dictionary.
class GlossaryFeature {
public:
    /// `chapters` are the book's texts, `language` its language.
    GlossaryFeature(Env& env, Book book, std::vector<std::string> chapters, std::string language);
    /// A run under way when the screen closes still becomes the dictionary.
    ~GlossaryFeature();

    void count();
    void start();
    /// Lets the requests in flight finish, and starts no more.
    void stop();

    bool isCounted() const { return isCounted_; }
    bool isRunning() const { return isRunning_; }
    int total() const { return total_; }
    int defined() const { return defined_; }
    const std::string& error() const { return error_; }
    /// The dictionary the glossary is.
    std::string name() const;
    /// How many words are defined, and what defining the rest should cost.
    std::string status() const;

    std::function<void()> onChange;

private:
    Env& env_;
    Book book_;
    std::vector<std::string> chapters_;
    std::string language_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::vector<BookWord> missing_;
    std::deque<std::vector<BookWord>> batches_;
    AiSettings settings_;
    bool isCounted_ = false;
    bool isRunning_ = false;
    bool stopping_ = false;
    int total_ = 0;
    int defined_ = 0;
    int inFlight_ = 0;
    std::string error_;

    void launch();
    void finishRun();
    void changed();
};
