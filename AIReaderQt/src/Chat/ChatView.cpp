#include "Chat/ChatView.hpp"

#include "Common/Ui.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

ChatView::ChatView(Env& env, Navigator& navigator, const Seed& seed)
    : Screen(navigator, "Chat"), feature_(env, seed.context, seed.scope), hint_(seed.hint) {
    auto* body = new QWidget;
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* holder = new QWidget;
    thread_ = Ui::column(holder, 10);
    scroller_ = Ui::scrolled(holder);
    layout->addWidget(scroller_, 1);
    layout->addWidget(Ui::separator());

    auto* composer = new QHBoxLayout;
    composer->setContentsMargins(12, 8, 12, 8);
    question_ = new QLineEdit;
    question_->setPlaceholderText("Ask a question");
    send_ = new QPushButton("Send");
    auto submit = [this] {
        std::string question = Ui::s(question_->text());
        question_->clear();
        feature_.send(question);
    };
    QObject::connect(question_, &QLineEdit::returnPressed, this, submit);
    QObject::connect(send_, &QPushButton::clicked, this, submit);
    composer->addWidget(question_, 1);
    composer->addWidget(send_);
    layout->addLayout(composer);
    setBody(body);
    setFocusProxy(question_);

    feature_.onChange = [this] { render(); };
    render();
}

void ChatView::render() {
    Ui::clear(thread_);
    if (feature_.turns().empty()) thread_->addWidget(Ui::rich("<i>" + Ui::escape(hint_) + "</i>"));
    for (const auto& turn : feature_.turns()) {
        if (turn.isReader) {
            QLabel* question = Ui::rich("<b>" + Ui::escape(turn.text) + "</b>");
            question->setAlignment(Qt::AlignRight);
            thread_->addWidget(question);
        } else {
            thread_->addWidget(Ui::label(turn.text));
        }
    }
    if (feature_.isAnswering()) thread_->addWidget(Ui::label("…"));
    if (!feature_.error().empty()) thread_->addWidget(Ui::note(Ui::escape(feature_.error())));
    send_->setEnabled(!feature_.isAnswering());
    // Keep the newest turn in view, once the new labels have a size.
    QPointer<QScrollBar> bar = scroller_->verticalScrollBar();
    Ui::later([bar] {
        if (bar) bar->setValue(bar->maximum());
    });
}
