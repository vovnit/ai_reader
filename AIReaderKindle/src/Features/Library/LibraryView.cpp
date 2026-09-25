#include "LibraryView.hpp"

#include "../Common/Navigator.hpp"
#include "../../Services/EpubLoader.hpp"
#include "../../Services/Paths.hpp"
#include "../../Support/Files.hpp"
#include "../Common/Widgets.hpp"
#include "../FilePicker/FilePickerView.hpp"
#include "../Reader/ReaderView.hpp"
#include "../Settings/SettingsView.hpp"
#include "../Words/WordsView.hpp"
#include "GroupView.hpp"
#include "LibraryFeature.hpp"

#include <gdk-pixbuf/gdk-pixbuf.h>

namespace LibraryView {

namespace {

int coverWidth() { return Widgets::px(60); }
int coverHeight() { return Widgets::px(90); }

/// Decodes a cover into a thumbnail; nothing when the book has none.
GdkPixbuf* thumbnail(const std::string& path) {
    auto bytes = EpubLoader::cover(path);
    if (!bytes) return nullptr;
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_set_size(loader, coverWidth(), coverHeight());
    bool ok = gdk_pixbuf_loader_write(loader, reinterpret_cast<const guchar*>(bytes->data()), bytes->size(), nullptr)
        && gdk_pixbuf_loader_close(loader, nullptr);
    GdkPixbuf* pixbuf = ok ? gdk_pixbuf_loader_get_pixbuf(loader) : nullptr;
    if (pixbuf) g_object_ref(pixbuf);
    g_object_unref(loader);
    return pixbuf;
}

struct CoverJob {
    GtkWidget* image;
    std::string path;
};

/// Covers are decoded one per idle pass, after the list is already on screen.
gboolean loadCover(gpointer data) {
    auto* job = static_cast<CoverJob*>(data);
    if (GTK_IS_WIDGET(job->image)) {
        if (GdkPixbuf* pixbuf = thumbnail(job->path)) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(job->image), pixbuf);
            g_object_unref(pixbuf);
        }
        g_object_unref(job->image);
    }
    delete job;
    return FALSE;  // one-shot
}

GtkWidget* row(Env& env, Navigator& navigator, LibraryFeature& feature, const Book& book) {
    GtkWidget* cover = gtk_image_new();
    gtk_widget_set_size_request(cover, coverWidth(), coverHeight());
    g_object_ref(cover);
    g_idle_add(loadCover, new CoverJob{cover, book.path});

    std::string text = "<b>" + Widgets::escape(book.title) + "</b>";
    if (!book.author.empty()) text += "\n" + Widgets::small(Widgets::escape(book.author));
    GtkWidget* content = gtk_hbox_new(FALSE, Widgets::px(12));
    gtk_box_pack_start(GTK_BOX(content), cover, FALSE, FALSE, 0);
    GtkWidget* caption = Widgets::markup(text);
    gtk_misc_set_alignment(GTK_MISC(caption), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(content), caption, TRUE, TRUE, 0);

    GtkWidget* open = Widgets::flatButton(content, [&env, &navigator, book] { ReaderView::open(env, navigator, book); });

    GtkWidget* remove = Widgets::glyphButton("✕", [&navigator, &feature, book] {
        Widgets::confirm(navigator.window(), "Remove “" + book.title + "”?",
                         "The book file is deleted. Words looked up in it are kept.", "Remove",
                         [&feature, book] { feature.remove(book); });
    }, true);
    GtkWidget* group = Widgets::button("Group", [&navigator, &feature, book] { GroupView::pick(navigator, feature, book); });
    GtkWidget* holder = gtk_vbox_new(FALSE, Widgets::px(6));
    gtk_box_pack_start(GTK_BOX(holder), remove, FALSE, FALSE, Widgets::px(8));
    gtk_box_pack_start(GTK_BOX(holder), group, FALSE, FALSE, 0);

    GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(4));
    gtk_box_pack_start(GTK_BOX(line), open, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(line), holder, FALSE, FALSE, 0);
    return line;
}

/// The line above a group's books: its name, and the way to dissolve it.
GtkWidget* heading(Navigator& navigator, LibraryFeature& feature, const BookGroup& group, size_t count) {
    std::string text = "<b>" + Widgets::escape(group.name) + "</b>  "
        + Widgets::small(std::to_string(count) + (count == 1 ? " book" : " books"));
    GtkWidget* caption = Widgets::markup(text);
    GtkWidget* dissolve = Widgets::glyphButton("✕", [&navigator, &feature, group] {
        Widgets::confirm(navigator.window(), "Dissolve “" + group.name + "”?",
                         "The books stay on the shelf, on their own.", "Dissolve",
                         [&feature, group] { feature.dissolve(group); });
    }, true);
    GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(4));
    gtk_box_pack_start(GTK_BOX(line), caption, TRUE, TRUE, Widgets::px(4));
    gtk_box_pack_end(GTK_BOX(line), dissolve, FALSE, FALSE, 0);
    return line;
}

void addBook(Navigator& navigator, LibraryFeature& feature) {
    FilePickerView::open(navigator, "Add a book",
        [](const std::string& path) { return Files::extension(path) == "epub"; },
        [&navigator, &feature](const std::string& path) {
            std::string error = feature.add(path);
            if (!error.empty()) Widgets::alert(navigator.window(), "Couldn’t add the book", error);
        });
}

}  // namespace

void open(Env& env, Navigator& navigator) {
    auto* feature = new LibraryFeature(env);
    GtkWidget* list = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(list), Widgets::px(8));

    auto render = [&env, &navigator, feature, list] {
        GList* children = gtk_container_get_children(GTK_CONTAINER(list));
        for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
        g_list_free(children);
        if (feature->books().empty()) {
            gtk_box_pack_start(GTK_BOX(list), Widgets::markup("No books yet.\n\n" + Widgets::small(
                "Tap Add to pick an <tt>.epub</tt> on this device, "
                "or copy files into\n<tt>" + Widgets::escape(Paths::books()) + "</tt>\n"
                "and tap Refresh.")), FALSE, FALSE, Widgets::px(12));
        }
        auto shelve = [&](const std::vector<Book>& books) {
            for (const auto& book : books) {
                gtk_box_pack_start(GTK_BOX(list), row(env, navigator, *feature, book), FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(list), Widgets::separator(), FALSE, FALSE, 0);
            }
        };
        // Each group under its name, then the books in none.
        for (const auto& group : feature->groups()) {
            std::vector<Book> books = feature->booksIn(group.id);
            gtk_box_pack_start(GTK_BOX(list), heading(navigator, *feature, group, books.size()), FALSE, FALSE, Widgets::px(6));
            shelve(books);
        }
        shelve(feature->booksIn(0));
        gtk_widget_show_all(list);
    };
    feature->onChange = render;

    std::vector<GtkWidget*> actions = {
        Widgets::button("Exit", [] { gtk_main_quit(); }),
        Widgets::button("Settings", [&env, &navigator] { SettingsView::open(env, navigator); }),
        Widgets::button("Words", [&env, &navigator] { WordsView::open(env, navigator, 0); }),
        Widgets::button("Refresh", [feature] { feature->refresh(); }),
        Widgets::button("Add", [&navigator, feature] { addBook(navigator, *feature); }),
    };
    GtkWidget* screen = Widgets::screen("Library", Widgets::scrolled(list), nullptr, "", actions);
    Widgets::own(screen, feature);
    // The folders may have changed while a book was open, and a book just
    // closed has a place worth sending on.
    Navigator::onReturn(screen, [feature] {
        feature->refresh();
        feature->sync();
    });
    navigator.push(screen);
}

}  // namespace LibraryView
