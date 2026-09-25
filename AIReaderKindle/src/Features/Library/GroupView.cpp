#include "GroupView.hpp"

#include "../Common/Keyboard.hpp"
#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "LibraryFeature.hpp"

namespace GroupView {

namespace {

const char* const none = "No group";
const char* const fresh = "New group…";

/// A screen to type the name of a new group into.
void create(Navigator& navigator, LibraryFeature& feature, const Book& book) {
    GtkWidget* entry = Widgets::entry("");
    auto submit = [&navigator, &feature, entry, book] {
        long long groupId = feature.groupNamed(Widgets::entryText(entry));
        if (!groupId) return;
        feature.assign(book, groupId);
        navigator.pop();
    };
    Widgets::connect(entry, "activate", submit);
    GtkWidget* composer = gtk_hbox_new(FALSE, Widgets::px(8));
    gtk_container_set_border_width(GTK_CONTAINER(composer), Widgets::px(8));
    gtk_box_pack_start(GTK_BOX(composer), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(composer), Widgets::button("Create", submit), FALSE, FALSE, 0);

    GtkWidget* hint = Widgets::markup("<i>" + Widgets::escape(
        "A series, an author, a course: books in one group are searched together.") + "</i>");
    gtk_misc_set_padding(GTK_MISC(hint), Widgets::px(12), Widgets::px(8));

    GtkWidget* body = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), composer, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), hint, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Keyboard::create(body), FALSE, FALSE, 0);

    navigator.push(Widgets::screen("New group", body, [&navigator] { navigator.pop(); }));
    gtk_widget_grab_focus(entry);
}

}  // namespace

void pick(Navigator& navigator, LibraryFeature& feature, const Book& book) {
    std::vector<std::string> options = {none};
    std::string current = none;
    for (const auto& group : feature.groups()) {
        options.push_back(group.name);
        if (group.id == book.groupId) current = group.name;
    }
    options.push_back(fresh);

    Widgets::picker(navigator, "Group for “" + book.title + "”", options, current,
        [&navigator, &feature, book](const std::string& choice) {
            if (choice == fresh) {
                create(navigator, feature, book);
                return;
            }
            long long groupId = 0;
            for (const auto& group : feature.groups()) {
                if (group.name == choice) groupId = group.id;
            }
            feature.assign(book, groupId);
        });
}

}  // namespace GroupView
