#include "XRay/XRayView.hpp"

#include "Chat/ChatView.hpp"
#include "Common/Navigator.hpp"
#include "Common/Ui.hpp"
#include "Domain/AI/ChatPrompt.hpp"
#include "Search/HitView.hpp"
#include "Support/Text.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QPushButton>
#include <QVBoxLayout>

XRayView::XRayView(Env& env, Navigator& navigator, const std::string& term, const ReaderLink& link)
    : Screen(navigator, Ui::q("X-ray: " + term)), env_(env), feature_(env, term, link.scope), link_(link) {
    auto* holder = new QWidget;
    column_ = Ui::column(holder, 14);
    setBody(Ui::scrolled(holder));
    feature_.onChange = [this] { render(); };
    render();
    feature_.start();
}

void XRayView::ask(Env& env, Navigator& navigator, const ReaderLink& link, QWidget* parent) {
    bool ok = false;
    QString term = QInputDialog::getText(parent, "X-ray",
        "A name, a place, a word the book uses its own way: what the book has said about it so far.",
        QLineEdit::Normal, "", &ok);
    std::string trimmed = Text::trim(Ui::s(term));
    if (ok && !trimmed.empty()) navigator.push(new XRayView(env, navigator, trimmed, link));
}

void XRayView::render() {
    Ui::clear(column_);
    if (feature_.isWorking()) {
        column_->addWidget(Ui::label("Reading the book…"));
    } else if (!feature_.answer().empty()) {
        column_->addWidget(Ui::rich("<big>" + Ui::escape(feature_.answer()) + "</big>"));
        ChatView::Seed seed{
            ChatPrompt::xrayContext(feature_.term(), feature_.answer()),
            "Ask about this — who they are to someone else, where it was first mentioned, what it stands for.",
            feature_.scope(),
        };
        auto* line = new QHBoxLayout;
        auto* ask = new QPushButton("Ask AI");
        QObject::connect(ask, &QPushButton::clicked, this, [this, seed] { navigator.push(new ChatView(env_, navigator, seed)); });
        line->addWidget(ask);
        line->addStretch();
        column_->addLayout(line);
    }
    if (!feature_.error().empty()) column_->addWidget(Ui::label(feature_.error()));
    if (feature_.isWorking()) return;

    column_->addWidget(Ui::separator());
    const auto& passages = feature_.passages();
    std::string caption = passages.empty()
        ? "Not met yet in what has been read."
        : "Where it has appeared so far" + std::string(link_.jump ? " — click to go there." : ".");
    column_->addWidget(Ui::note(Ui::escape(caption)));
    bool severalBooks = feature_.scope().corpus && feature_.scope().corpus->severalBooks();
    for (const auto& hit : passages) {
        column_->addWidget(HitView::create(hit, severalBooks, link_.jump));
        column_->addWidget(Ui::separator());
    }
}
