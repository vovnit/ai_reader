#pragma once

#include "../../Domain/AI/WordExplanation.hpp"
#include "../../Domain/Dictionary/DictionaryLookup.hpp"
#include "../../Services/BookCorpus.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

/// One word lookup: read the cache, otherwise ask the explainer and store the
/// answer. The dictionary's own entry for the lemma comes with it, so the
/// reader can see what the answer was drawn from.
class LookupFeature {
public:
    /// `scope` is what the model may search while explaining; none from a
    /// screen with no book open.
    LookupFeature(Env& env, LookupContext context, ReadingScope scope);

    void start();

    const LookupContext& context() const { return context_; }
    const ReadingScope& scope() const { return scope_; }
    const std::optional<WordExplanation>& explanation() const { return explanation_; }
    /// The articles under the lemma; empty when the dictionary has no such
    /// headword, as after a guess.
    const std::vector<DictionaryLookup::Article>& entry() const { return entry_; }
    const std::string& error() const { return error_; }

    std::function<void()> onChange;

private:
    Env& env_;
    LookupContext context_;
    ReadingScope scope_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::optional<WordExplanation> explanation_;
    std::vector<DictionaryLookup::Article> entry_;
    std::string error_;
};
