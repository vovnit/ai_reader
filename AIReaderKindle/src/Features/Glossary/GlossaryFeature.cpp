#include "GlossaryFeature.hpp"

#include "../../Domain/AI/GlossaryPrompt.hpp"
#include "../../Services/Glossary.hpp"
#include "../../Support/Async.hpp"
#include "../../Support/Text.hpp"

#include <algorithm>
#include <set>

namespace {

/// Requests in flight at once.
const int parallel = 4;

}  // namespace

GlossaryFeature::GlossaryFeature(Env& env, Book book, std::vector<std::string> chapters, std::string language)
    : env_(env), book_(std::move(book)), chapters_(std::move(chapters)), language_(std::move(language)) {}

GlossaryFeature::~GlossaryFeature() {
    if (isRunning_) Glossary::install(env_.packs, book_);
}

std::string GlossaryFeature::name() const {
    return Glossary::name(book_);
}

std::string GlossaryFeature::status() const {
    int left = total_ - defined_;
    if (isRunning_) return "Defined " + Text::thousands(defined_) + " of " + Text::thousands(total_) + " words.";
    if (left == 0) return "All " + Text::thousands(total_) + " words are defined.";
    std::string done = defined_ > 0
        ? Text::thousands(defined_) + " of " + Text::thousands(total_) + " words are defined; the rest"
        : Text::thousands(total_) + " words";
    return done + " should take about " + Text::thousands(left * GlossaryPrompt::tokensPerWord)
        + " tokens with " + env_.settings.ai().model + ".";
}

void GlossaryFeature::count() {
    struct Counted {
        std::vector<BookWord> missing;
        int total = 0;
    };
    std::vector<std::string> chapters = chapters_;
    std::string language = language_;
    Book book = book_;
    Async::run<Counted>(
        [chapters, language, book] {
            Counted counted;
            std::vector<BookWord> words = BookWords::collect(chapters, language);
            std::set<std::string> defined = Glossary::definedForms(book);
            counted.total = static_cast<int>(words.size());
            for (auto& word : words) {
                if (!defined.count(word.form)) counted.missing.push_back(std::move(word));
            }
            return counted;
        },
        [this](Counted counted) {
            missing_ = std::move(counted.missing);
            total_ = counted.total;
            defined_ = total_ - static_cast<int>(missing_.size());
            isCounted_ = true;
            changed();
        },
        alive_);
}

void GlossaryFeature::start() {
    if (isRunning_ || missing_.empty()) return;
    isRunning_ = true;
    stopping_ = false;
    error_.clear();
    settings_ = env_.settings.ai();
    batches_.clear();
    for (size_t start = 0; start < missing_.size(); start += GlossaryPrompt::batchSize) {
        size_t end = std::min(missing_.size(), start + GlossaryPrompt::batchSize);
        batches_.emplace_back(missing_.begin() + start, missing_.begin() + end);
    }
    for (int i = 0; i < parallel; ++i) launch();
    changed();
}

void GlossaryFeature::stop() {
    stopping_ = true;
}

void GlossaryFeature::launch() {
    if (stopping_ || batches_.empty()) {
        if (inFlight_ == 0 && isRunning_) finishRun();
        return;
    }
    std::vector<BookWord> batch = std::move(batches_.front());
    batches_.pop_front();
    ++inFlight_;

    struct Answer {
        std::set<std::string> forms;
        std::string error;
    };
    AiSettings settings = settings_;
    Book book = book_;
    Async::run<Answer>(
        [batch, settings, book] {
            Answer answer;
            try {
                auto definitions = Glossary::define(settings, batch);
                // Kept here rather than on the screen's side, so what was
                // paid for is kept even if the screen has closed.
                Glossary::append(book, settings, definitions);
                for (const auto& definition : definitions) answer.forms.insert(definition.first);
            } catch (const std::exception& failure) {
                answer.error = failure.what();
            }
            return answer;
        },
        [this](Answer answer) {
            --inFlight_;
            if (!answer.error.empty()) {
                // The rest would most likely fail the same way.
                error_ = answer.error;
                stopping_ = true;
            }
            missing_.erase(std::remove_if(missing_.begin(), missing_.end(),
                [&](const BookWord& word) { return answer.forms.count(word.form) > 0; }), missing_.end());
            defined_ += static_cast<int>(answer.forms.size());
            launch();
            changed();
        },
        alive_);
}

void GlossaryFeature::finishRun() {
    isRunning_ = false;
    Glossary::install(env_.packs, book_);
    changed();
}

void GlossaryFeature::changed() {
    if (onChange) onChange();
}
