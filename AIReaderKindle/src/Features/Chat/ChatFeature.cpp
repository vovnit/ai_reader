#include "ChatFeature.hpp"

#include "../../Services/ToolRunner.hpp"
#include "../../Support/Async.hpp"
#include "../../Support/Text.hpp"

ChatFeature::ChatFeature(Env& env, std::string context, ReadingScope scope)
    : env_(env), context_(std::move(context)), scope_(std::move(scope)) {}

void ChatFeature::send(const std::string& question) {
    std::string trimmed = Text::trim(question);
    if (trimmed.empty() || isAnswering_) return;
    turns_.push_back({true, trimmed});
    isAnswering_ = true;
    error_.clear();
    if (onChange) onChange();

    struct Reply {
        std::string text;
        std::string error;
    };
    AiSettings settings = env_.settings.ai();
    std::vector<ChatMessage> messages = ChatPrompt::messages(context_, turns_, settings.language);
    ToolRunner::Tools tools{scope_, env_.packs.enabled(), env_.settings.webSearch()};
    Async::run<Reply>(
        [messages, settings, tools]() mutable {
            Reply reply;
            try {
                reply.text = ToolRunner::converse(settings, messages, ChatPrompt::tools(), false, tools).content.value_or("");
            } catch (const std::exception& failure) {
                reply.error = failure.what();
            }
            return reply;
        },
        [this](Reply reply) {
            isAnswering_ = false;
            if (reply.error.empty()) turns_.push_back({false, reply.text});
            else error_ = reply.error;
            if (onChange) onChange();
        },
        alive_);
}
