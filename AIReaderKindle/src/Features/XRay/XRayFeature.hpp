#pragma once

#include "../../Domain/Search/BookSearch.hpp"
#include "../../Services/BookCorpus.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

/// What the book itself says a name or a word is: the passages where it has
/// appeared so far are gathered and the model is asked to read them. Nothing
/// past the page on screen is shown or sent, so nothing is given away.
class XRayFeature {
public:
    XRayFeature(Env& env, std::string term, ReadingScope scope);

    void start();

    const std::string& term() const { return term_; }
    const ReadingScope& scope() const { return scope_; }
    /// The passages the answer was drawn from, in reading order.
    const std::vector<SearchHit>& passages() const { return passages_; }
    const std::string& answer() const { return answer_; }
    const std::string& error() const { return error_; }
    bool isWorking() const { return isWorking_; }

    std::function<void()> onChange;

private:
    Env& env_;
    std::string term_;
    ReadingScope scope_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::vector<SearchHit> passages_;
    std::string answer_;
    std::string error_;
    bool isWorking_ = false;
};
