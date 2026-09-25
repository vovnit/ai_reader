#include "LookupFeature.hpp"

#include "../../Services/ChatApi.hpp"
#include "../../Services/DictionaryDatabase.hpp"
#include "../../Services/WordExplainer.hpp"
#include "../../Support/Async.hpp"

LookupFeature::LookupFeature(Env& env, LookupContext context, ReadingScope scope)
    : env_(env), context_(std::move(context)), scope_(std::move(scope)) {}

void LookupFeature::start() {
    if (explanation_) return;
    ToolRunner::Tools tools{scope_, env_.packs.enabled(), env_.settings.webSearch()};
    if (auto cached = env_.lookups.cached(context_)) {
        explanation_ = cached;
        entry_ = DictionaryDatabase::shared().articlesFor(cached->lemma, tools.packs);
        if (onChange) onChange();
        return;
    }

    struct Answer {
        std::optional<WordExplanation> explanation;
        std::vector<DictionaryLookup::Article> entry;
        std::string error;
    };
    AiSettings settings = env_.settings.ai();
    LookupContext context = context_;
    Async::run<Answer>(
        [context, settings, tools] {
            Answer answer;
            try {
                answer.explanation = WordExplainer::explain(context.word, context.sentence, settings, tools);
                answer.entry = DictionaryDatabase::shared().articlesFor(answer.explanation->lemma, tools.packs);
            } catch (const std::exception& failure) {
                answer.error = failure.what();
            }
            return answer;
        },
        [this](Answer answer) {
            if (answer.explanation) {
                env_.lookups.save(context_, *answer.explanation);
                explanation_ = answer.explanation;
                entry_ = std::move(answer.entry);
            } else {
                error_ = answer.error;
            }
            if (onChange) onChange();
        },
        alive_);
}
