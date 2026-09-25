#include "Lookup/LookupView.hpp"

#include "Chat/ChatView.hpp"
#include "Common/Navigator.hpp"
#include "Common/Ui.hpp"
#include "Domain/AI/ChatPrompt.hpp"
#include "Lookup/EntryView.hpp"
#include "XRay/XRayView.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

LookupView::LookupView(Env& env, Navigator& navigator, const LookupContext& context, const ReaderLink& link)
    : Screen(navigator, Ui::q(context.word)), env_(env), feature_(env, context, link.scope), link_(link) {
    auto* holder = new QWidget;
    column_ = Ui::column(holder, 14);
    setBody(Ui::scrolled(holder));
    feature_.onChange = [this] { render(); };
    render();
    feature_.start();
}

void LookupView::render() {
    Ui::clear(column_);
    const LookupContext& context = feature_.context();
    auto sentence = [&] {
        if (!context.sentence.empty()) column_->addWidget(Ui::rich("<i>" + Ui::escape(context.sentence) + "</i>"));
    };

    const auto& explanation = feature_.explanation();
    if (!explanation) {
        column_->addWidget(Ui::label(feature_.error().empty() ? "Looking up…" : feature_.error()));
        if (feature_.error().empty()) sentence();
        return;
    }

    // What the word means here is what the reader came for.
    column_->addWidget(Ui::rich("<span style=\"font-size: x-large\">" + Ui::escape(explanation->meaning) + "</span>"));
    column_->addWidget(Ui::rich("<b>" + Ui::escape(explanation->lemma) + "</b><br>" + Ui::small(Ui::escape(explanation->formNote))));

    auto* actions = new QHBoxLayout;
    auto action = [&](const QString& label, std::function<void()> run) {
        auto* button = new QPushButton(label);
        QObject::connect(button, &QPushButton::clicked, this, [run = std::move(run)] { run(); });
        actions->addWidget(button);
    };
    // Only when the dictionary really has the lemma; a guess has no entry.
    if (!feature_.entry().empty()) {
        std::string lemma = explanation->lemma;
        auto articles = feature_.entry();
        action("Dictionary entry", [this, lemma, articles] { navigator.push(new EntryView(navigator, lemma, articles)); });
    }
    // What the book, rather than the dictionary, says the word is.
    if (link_.scope.corpus) {
        std::string word = context.word;
        action("X-ray", [this, word] { navigator.push(new XRayView(env_, navigator, word, link_)); });
    }
    ChatView::Seed seed{
        ChatPrompt::wordContext(context.word, context.sentence, *explanation),
        "Ask about this word — another example, a nuance, how it differs from a similar one.",
        feature_.scope(),
    };
    action("Ask AI", [this, seed] { navigator.push(new ChatView(env_, navigator, seed)); });
    actions->addStretch();
    column_->addLayout(actions);

    if (!context.sentence.empty()) {
        column_->addWidget(Ui::separator());
        sentence();
    }
    if (explanation->guessed || explanation->confidence < 0.6) {
        column_->addWidget(Ui::separator());
        std::string note = explanation->guessed ? "Догадка, не из словаря\n" : "";
        note += "Уверенность: " + std::to_string(static_cast<int>(explanation->confidence * 100 + 0.5)) + "%";
        column_->addWidget(Ui::note(Ui::escape(note)));
    }
}
