#include "XRayFeature.hpp"

#include "../../Domain/AI/SearchTool.hpp"
#include "../../Domain/AI/XRayPrompt.hpp"
#include "../../Services/ToolRunner.hpp"
#include "../../Support/Async.hpp"

XRayFeature::XRayFeature(Env& env, std::string term, ReadingScope scope)
    : env_(env), term_(std::move(term)), scope_(std::move(scope)) {}

void XRayFeature::start() {
    if (isWorking_ || !answer_.empty()) return;
    isWorking_ = true;
    if (onChange) onChange();

    struct Result {
        std::vector<SearchHit> passages;
        std::string answer;
        std::string error;
    };
    std::string term = term_;
    AiSettings settings = env_.settings.ai();
    ToolRunner::Tools tools{scope_, {}, env_.settings.webSearch()};
    Async::run<Result>(
        [term, settings, tools] {
            Result result;
            if (!tools.scope.corpus) {
                result.error = "No book is open to read from.";
                return result;
            }
            result.passages = tools.scope.corpus->search(term, XRayPrompt::passageLimit, tools.scope.upTo);
            try {
                std::vector<ChatMessage> messages = XRayPrompt::messages(term, result.passages, tools.scope.corpus->severalBooks(), settings.language);
                Json offered = Json(std::vector<Json>{SearchTool::tool()});
                result.answer = ToolRunner::converse(settings, messages, offered, false, tools).content.value_or("");
            } catch (const std::exception& failure) {
                result.error = failure.what();
            }
            for (const auto& problem : tools.scope.corpus->errors()) {
                result.error += (result.error.empty() ? "" : "\n") + problem;
            }
            return result;
        },
        [this](Result result) {
            isWorking_ = false;
            passages_ = std::move(result.passages);
            answer_ = std::move(result.answer);
            error_ = std::move(result.error);
            if (onChange) onChange();
        },
        alive_);
}
