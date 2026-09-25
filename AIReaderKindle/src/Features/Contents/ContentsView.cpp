#include "ContentsView.hpp"

#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "../Reader/ReaderFeature.hpp"

#include <algorithm>

namespace ContentsView {

namespace {

struct Reveal {
    GtkWidget* row;
    GtkWidget* scroller;
};

/// Scrolls the list so `row` sits a row below the top, once it has a place.
void reveal(GtkWidget* row, GtkWidget* scroller) {
    g_object_ref(row);
    g_object_ref(scroller);
    g_idle_add([](gpointer data) -> gboolean {
        auto* reveal = static_cast<Reveal*>(data);
        GtkAdjustment* vertical = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(reveal->scroller));
        gtk_adjustment_set_value(vertical, std::max(0.0, reveal->row->allocation.y - static_cast<double>(Widgets::px(48))));
        g_object_unref(reveal->row);
        g_object_unref(reveal->scroller);
        delete reveal;
        return FALSE;  // one-shot
    }, new Reveal{row, scroller});
}

}  // namespace

void open(Navigator& navigator, ReaderFeature& reader, const ReaderLink& link) {
    GtkWidget* list = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(list), Widgets::px(8));

    long long bookId = reader.book().id;
    int current = reader.contentsEntry();
    GtkWidget* currentRow = nullptr;
    const auto& entries = reader.contents();
    for (size_t i = 0; i < entries.size(); ++i) {
        const ContentsEntry& entry = entries[i];
        GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(8));
        gtk_container_set_border_width(GTK_CONTAINER(line), Widgets::px(4));
        // Each level of nesting steps in by an indent.
        GtkWidget* indent = gtk_alignment_new(0, 0, 1, 1);
        gtk_alignment_set_padding(GTK_ALIGNMENT(indent), 0, 0, Widgets::px(20) * entry.depth, 0);
        gtk_container_add(GTK_CONTAINER(indent), Widgets::label(entry.title));
        gtk_box_pack_start(GTK_BOX(line), indent, TRUE, TRUE, 0);
        if (static_cast<int>(i) == current) gtk_box_pack_end(GTK_BOX(line), Widgets::label("✓", 1, false), FALSE, FALSE, 0);

        BookPosition position{bookId, entry.chapter, entry.offset};
        GtkWidget* row = Widgets::flatButton(line, [jump = link.jump, position] { jump(position); });
        gtk_widget_set_size_request(row, -1, Widgets::px(48));
        if (static_cast<int>(i) == current) currentRow = row;
        gtk_box_pack_start(GTK_BOX(list), row, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(list), Widgets::separator(), FALSE, FALSE, 0);
    }
    if (entries.empty()) {
        gtk_box_pack_start(GTK_BOX(list), Widgets::label("This book has no table of contents."), FALSE, FALSE, 0);
    }

    GtkWidget* scroller = Widgets::scrolled(list);
    navigator.push(Widgets::screen("Contents", scroller, [&navigator] { navigator.pop(); }));
    // The book lists what is being read; a long list opens there rather
    // than at the top.
    if (currentRow) reveal(currentRow, scroller);
}

}  // namespace ContentsView
