#include "Navigator.hpp"

#include "Widgets.hpp"

Navigator::Navigator() {
    notebook_ = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook_), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook_), FALSE);
}

GtkWindow* Navigator::window() const {
    GtkWidget* top = gtk_widget_get_toplevel(notebook_);
    return GTK_IS_WINDOW(top) ? GTK_WINDOW(top) : nullptr;
}

void Navigator::push(GtkWidget* screen) {
    gtk_widget_show_all(screen);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), screen, nullptr);
    showTop();
}

// Pops are deferred to the main loop: a screen usually asks to be popped
// from one of its own buttons, and destroying it mid-click is not safe.
void Navigator::pop() {
    g_idle_add([](gpointer data) -> gboolean {
        auto* self = static_cast<Navigator*>(data);
        int count = self->depth();
        if (count > 1) gtk_notebook_remove_page(GTK_NOTEBOOK(self->notebook_), count - 1);
        self->showTop();
        return FALSE;  // one-shot
    }, this);
}

void Navigator::popToRoot() {
    g_idle_add([](gpointer data) -> gboolean {
        auto* self = static_cast<Navigator*>(data);
        while (self->depth() > 1) gtk_notebook_remove_page(GTK_NOTEBOOK(self->notebook_), self->depth() - 1);
        self->showTop();
        return FALSE;  // one-shot
    }, this);
}

void Navigator::popTo(const std::string& tag) {
    struct Request {
        Navigator* self;
        std::string tag;
    };
    g_idle_add([](gpointer data) -> gboolean {
        auto* request = static_cast<Request*>(data);
        GtkNotebook* notebook = GTK_NOTEBOOK(request->self->notebook_);
        int target = -1;
        for (int index = request->self->depth() - 1; index >= 0 && target < 0; --index) {
            const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(gtk_notebook_get_nth_page(notebook, index)), "aireader-tag"));
            if (name && request->tag == name) target = index;
        }
        if (target >= 0) {
            while (request->self->depth() > target + 1) gtk_notebook_remove_page(notebook, request->self->depth() - 1);
            request->self->showTop();
        }
        delete request;
        return FALSE;  // one-shot
    }, new Request{this, tag});
}

int Navigator::depth() const {
    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_));
}

void Navigator::onReturn(GtkWidget* screen, std::function<void()> callback) {
    auto* stored = new std::function<void()>(std::move(callback));
    g_object_set_data_full(G_OBJECT(screen), "aireader-on-return", stored,
                           [](gpointer pointer) { delete static_cast<std::function<void()>*>(pointer); });
}

void Navigator::tag(GtkWidget* screen, const std::string& tag) {
    g_object_set_data_full(G_OBJECT(screen), "aireader-tag", g_strdup(tag.c_str()), g_free);
}

void Navigator::showTop() {
    int count = depth();
    if (count == 0) return;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), count - 1);
    GtkWidget* top = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook_), count - 1);
    auto* callback = static_cast<std::function<void()>*>(g_object_get_data(G_OBJECT(top), "aireader-on-return"));
    if (callback) (*callback)();
}
