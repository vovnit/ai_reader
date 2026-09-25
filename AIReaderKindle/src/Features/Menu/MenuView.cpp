#include "MenuView.hpp"

#include "../../Domain/AI/ChatPrompt.hpp"
#include "../Chat/ChatView.hpp"
#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "../Contents/ContentsView.hpp"
#include "../Reader/ReaderFeature.hpp"
#include "../Search/SearchView.hpp"
#include "../Words/WordsView.hpp"
#include "../XRay/XRayView.hpp"
#include "DisplayView.hpp"

namespace MenuView {

void open(Env& env, Navigator& navigator, ReaderFeature& reader, const ReaderLink& link) {
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(10));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(16));
    auto add = [&](const std::string& label, Widgets::Action action) {
        GtkWidget* button = Widgets::button(label, std::move(action));
        gtk_widget_set_size_request(button, -1, Widgets::px(56));
        gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    };

    long long bookId = reader.book().id;
    std::string page = reader.pageText();
    // A search covers the group when the book is in one.
    auto group = reader.group();
    std::string covers = group
        ? "the " + std::to_string(reader.corpus()->books().size()) + " books of “" + group->name + "”"
        : "this book";
    ChatView::Seed pageSeed{
        ChatPrompt::pageContext(page),
        "Ask about this page — a sentence you can’t parse, a word’s role, what is going on.",
        link.scope,
    };

    add("Contents", [&navigator, &reader, link] { ContentsView::open(navigator, reader, link); });
    add("Lookups", [&env, &navigator, bookId] { WordsView::open(env, navigator, bookId); });
    add("Search", [&navigator, link, covers] { SearchView::open(navigator, link, covers); });
    add("X-ray", [&env, &navigator, link] { XRayView::ask(env, navigator, link); });
    add("Ask about this page", [&env, &navigator, pageSeed] { ChatView::open(env, navigator, pageSeed); });
    add("Display", [&env, &navigator, &reader] { DisplayView::open(env, navigator, reader); });
    add("Close book", [&navigator] { navigator.popToRoot(); });

    navigator.push(Widgets::screen("Menu", box, [&navigator] { navigator.pop(); }, "Done"));
}

}  // namespace MenuView
