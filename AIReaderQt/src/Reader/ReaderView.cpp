#include "Reader/ReaderView.hpp"

#include "Chat/ChatView.hpp"
#include "Common/Navigator.hpp"
#include "Common/Ui.hpp"
#include "Domain/AI/ChatPrompt.hpp"
#include "Lookup/LookupView.hpp"
#include "Menu/ContentsView.hpp"
#include "Menu/DisplayView.hpp"
#include "Reader/PageView.hpp"
#include "Search/SearchView.hpp"
#include "Words/WordsView.hpp"
#include "XRay/XRayView.hpp"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace {

/// Lines much longer than this are hard to follow, so a wide window keeps
/// the page to a column in the middle.
constexpr int widestPage = 820;

}  // namespace

ReaderView::ReaderView(Env& env, Navigator& navigator, const Book& book)
    : Screen(navigator, Ui::q(book.title), "Library"), env_(env), feature_(env, book) {
    tag = "reader";
    auto* body = new QWidget;
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    page_ = new PageView(feature_);
    page_->setMaximumWidth(widestPage);
    page_->onTap = [this](const ReaderFeature::Tap& tap) { tapped(tap); };
    auto* centered = new QHBoxLayout;
    centered->addWidget(page_);
    layout->addLayout(centered, 1);
    layout->addWidget(Ui::separator());

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(8, 4, 8, 4);
    auto* previous = new QPushButton("‹");
    auto* next = new QPushButton("›");
    progress_ = new QPushButton(Ui::q(book.title));
    progress_->setFlat(true);
    progress_->setToolTip("Menu");
    QObject::connect(previous, &QPushButton::clicked, this, [this] { feature_.previous(); });
    QObject::connect(next, &QPushButton::clicked, this, [this] { feature_.next(); });
    QObject::connect(progress_, &QPushButton::clicked, this, [this] { showMenu(); });
    for (auto* button : {previous, next}) button->setFocusPolicy(Qt::NoFocus);
    footer->addWidget(previous);
    footer->addWidget(progress_, 1);
    footer->addWidget(next);
    layout->addLayout(footer);
    setBody(body);
    setFocusProxy(page_);

    auto* search = new QShortcut(QKeySequence::Find, this);
    QObject::connect(search, &QShortcut::activated, this, [this] {
        this->navigator.push(new SearchView(this->navigator, link(), covers()));
    });

    feature_.onChange = [this] { render(); };
    feature_.load();
}

void ReaderView::returned() {
    feature_.setStyle(env_.settings.style());
}

void ReaderView::render() {
    QString text = Ui::q(feature_.book().title);
    if (feature_.status() == ReaderFeature::Status::Loaded && feature_.pageCount() > 0) {
        text = QString("Chapter %1/%2  ·  Page %3/%4")
            .arg(feature_.chapterIndex() + 1).arg(feature_.chapterCount())
            .arg(feature_.pageIndex() + 1).arg(feature_.pageCount());
    }
    progress_->setText(text);
    page_->refresh();
}

void ReaderView::tapped(const ReaderFeature::Tap& tap) {
    if (tap.kind != ReaderFeature::Tap::Kind::Word) return;
    LookupContext context{tap.selection.word, tap.selection.sentence, feature_.language(), feature_.book().id};
    navigator.push(new LookupView(env_, navigator, context, link()));
}

void ReaderView::showMenu() {
    ReaderLink link = this->link();
    long long bookId = feature_.book().id;
    std::string covers = this->covers();
    ChatView::Seed pageSeed{
        ChatPrompt::pageContext(feature_.pageText()),
        "Ask about this page — a sentence you can’t parse, a word’s role, what is going on.",
        link.scope,
    };

    QMenu menu(this);
    auto add = [&](const QString& label, std::function<void()> action) {
        QObject::connect(menu.addAction(label), &QAction::triggered, this, [action = std::move(action)] { action(); });
    };
    add("Contents", [this, link] { navigator.push(new ContentsView(navigator, feature_, link)); });
    add("Lookups", [this, bookId] { navigator.push(new WordsView(env_, navigator, bookId)); });
    add("Search", [this, link, covers] { navigator.push(new SearchView(navigator, link, covers)); });
    add("X-ray…", [this, link] { XRayView::ask(env_, navigator, link, this); });
    add("Ask about this page", [this, pageSeed] { navigator.push(new ChatView(env_, navigator, pageSeed)); });
    add("Display", [this] { navigator.push(new DisplayView(env_, navigator, feature_)); });
    menu.addSeparator();
    add("Close book", [this] { navigator.popToRoot(); });
    menu.exec(progress_->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu.sizeHint().height()));
}

std::string ReaderView::covers() const {
    // A search covers the group when the book is in one.
    auto group = feature_.group();
    return group
        ? "the " + std::to_string(feature_.corpus()->books().size()) + " books of “" + group->name + "”"
        : "this book";
}

ReaderLink ReaderView::link() {
    Env& env = env_;
    Navigator& navigator = this->navigator;
    ReaderFeature& reader = feature_;
    return {feature_.scope(), [&env, &navigator, &reader](const BookPosition& position) {
        if (position.bookId == reader.book().id) {
            reader.goTo(position.chapter, position.offset);
            navigator.popTo("reader");
            return;
        }
        // Another book of the group: close this one and open that, there.
        auto other = env.library.find(position.bookId);
        if (!other) return;
        other->readingChapter = position.chapter;
        other->readingOffset = position.offset;
        navigator.popToRoot();
        navigator.push(new ReaderView(env, navigator, *other));
    }};
}
