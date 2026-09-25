#include "HitView.hpp"

#include "../Common/Widgets.hpp"

namespace HitView {

GtkWidget* create(const SearchHit& hit, bool showBook, std::function<void(const BookPosition&)> onTap) {
    const std::string& text = hit.excerpt;
    std::string markup = Widgets::escape(text.substr(0, hit.matchStart))
        + "<b>" + Widgets::escape(text.substr(hit.matchStart, hit.matchEnd - hit.matchStart)) + "</b>"
        + Widgets::escape(text.substr(hit.matchEnd));
    std::string where = "Chapter " + std::to_string(hit.chapter + 1);
    if (showBook) where = hit.bookTitle + " · " + where;

    GtkWidget* content = gtk_vbox_new(FALSE, Widgets::px(3));
    gtk_box_pack_start(GTK_BOX(content), Widgets::markup(markup), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), Widgets::markup(Widgets::small(Widgets::escape(where))), FALSE, FALSE, 0);
    if (!onTap) return content;

    BookPosition position{hit.bookId, hit.chapter, hit.offset};
    return Widgets::flatButton(content, [onTap, position] { onTap(position); });
}

}  // namespace HitView
