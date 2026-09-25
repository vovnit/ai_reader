#include "Keyboard.hpp"

#include "Theme.hpp"
#include "Widgets.hpp"

#include <string>
#include <vector>

namespace Keyboard {

namespace {

enum class Layout { Latin, Cyrillic, Symbols };

/// A row is keys separated by spaces; a key in braces is special.
const std::vector<std::vector<std::string>> layouts[] = {
    {   // Latin, with the accents French needs
        {"é", "è", "ê", "à", "ç", "ù", "â", "î", "ô", "û", "ë", "ï", "œ"},
        {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
        {"a", "s", "d", "f", "g", "h", "j", "k", "l", "'"},
        {"{shift}", "z", "x", "c", "v", "b", "n", "m", ",", ".", "{backspace}"},
        {"{symbols}", "{layout}", "{space}", "?", "{enter}"},
    },
    {   // Cyrillic
        {"й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з", "х", "ъ"},
        {"ф", "ы", "в", "а", "п", "р", "о", "л", "д", "ж", "э"},
        {"{shift}", "я", "ч", "с", "м", "и", "т", "ь", "б", "ю", "ё", "{backspace}"},
        {"{symbols}", "{layout}", "{space}", ",", ".", "{enter}"},
    },
    {   // Digits and punctuation
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
        {"-", "/", ":", ";", "(", ")", "€", "&", "@", "\""},
        {"!", "?", "«", "»", "—", "'", "+", "=", "%", "{backspace}"},
        {"{symbols}", "{layout}", "{space}", "{enter}"},
    },
};

struct KeyButton {
    GtkWidget* button;
    std::string key;
};

struct State {
    GtkWidget* toplevel;
    GtkWidget* box;
    Layout layout = Layout::Latin;
    /// Which alphabet the symbols page returns to.
    Layout alphabet = Layout::Latin;
    bool shift = false;
    /// One box per layout, built once; only one is shown at a time.
    GtkWidget* pages[3] = {nullptr, nullptr, nullptr};
    std::vector<KeyButton> keys;

    GtkWidget* entry() const {
        GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(gtk_widget_get_toplevel(toplevel)));
        return focus && GTK_IS_ENTRY(focus) ? focus : nullptr;
    }

    void insert(const std::string& text) {
        GtkWidget* target = entry();
        if (!target) return;
        gint position = gtk_editable_get_position(GTK_EDITABLE(target));
        gtk_editable_insert_text(GTK_EDITABLE(target), text.c_str(), static_cast<gint>(text.size()), &position);
        gtk_editable_set_position(GTK_EDITABLE(target), position);
    }

    void backspace() {
        GtkWidget* target = entry();
        if (!target) return;
        gint start, end;
        if (!gtk_editable_get_selection_bounds(GTK_EDITABLE(target), &start, &end)) {
            end = gtk_editable_get_position(GTK_EDITABLE(target));
            start = end > 0 ? end - 1 : 0;
        }
        gtk_editable_delete_text(GTK_EDITABLE(target), start, end);
    }

    static std::string upper(const std::string& text) {
        gchar* up = g_utf8_strup(text.c_str(), -1);
        std::string result = up;
        g_free(up);
        return result;
    }

    std::string label(const std::string& key) const {
        if (key == "{shift}") return shift ? "SHIFT" : "Shift";
        if (key == "{backspace}") return "Del";
        if (key == "{space}") return " ";
        if (key == "{enter}") return "Enter";
        if (key == "{symbols}") return layout == Layout::Symbols ? "ABC" : "123";
        if (key == "{layout}") return alphabet == Layout::Latin ? "RU" : "EN";
        return shift ? upper(key) : key;
    }

    /// Labels follow the state; the widgets stay where they are.
    void relabel() {
        for (const auto& key : keys) gtk_button_set_label(GTK_BUTTON(key.button), label(key.key).c_str());
    }

    void show(Layout wanted) {
        layout = wanted;
        for (int i = 0; i < 3; ++i) {
            if (i == static_cast<int>(layout)) gtk_widget_show(pages[i]); else gtk_widget_hide(pages[i]);
        }
        relabel();
    }

    void press(const std::string& key) {
        if (key == "{shift}") {
            shift = !shift;
            relabel();
        } else if (key == "{backspace}") {
            backspace();
        } else if (key == "{space}") {
            insert(" ");
        } else if (key == "{enter}") {
            if (GtkWidget* target = entry()) g_signal_emit_by_name(target, "activate");
        } else if (key == "{symbols}") {
            show(layout == Layout::Symbols ? alphabet : Layout::Symbols);
        } else if (key == "{layout}") {
            alphabet = alphabet == Layout::Latin ? Layout::Cyrillic : Layout::Latin;
            show(alphabet);
        } else {
            insert(shift ? upper(key) : key);
            if (shift) {
                shift = false;
                relabel();
            }
        }
    }

    void build() {
        for (int i = 0; i < 3; ++i) {
            GtkWidget* page = gtk_vbox_new(FALSE, Widgets::px(3));
            for (const auto& row : layouts[i]) {
                GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(3));
                for (const auto& key : row) {
                    GtkWidget* button = Widgets::button(label(key), [this, key] { press(key); });
                    Theme::styleKey(button);
                    // Keys must not take the focus away from the entry being typed into.
                    gtk_widget_set_can_focus(button, FALSE);
                    gtk_widget_set_size_request(button, key == "{space}" ? Widgets::px(160) : Widgets::px(24), Widgets::px(44));
                    gtk_box_pack_start(GTK_BOX(line), button, TRUE, TRUE, 0);
                    keys.push_back({button, key});
                }
                gtk_box_pack_start(GTK_BOX(page), line, FALSE, FALSE, 0);
            }
            gtk_widget_show_all(page);
            gtk_widget_set_no_show_all(page, TRUE);
            gtk_box_pack_start(GTK_BOX(box), page, FALSE, FALSE, 0);
            pages[i] = page;
        }
        show(Layout::Latin);
    }
};

}  // namespace

GtkWidget* create(GtkWidget* toplevel) {
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(3));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(4));
    auto* state = new State{toplevel, box};
    Widgets::own(box, state);
    state->build();
    return box;
}

}  // namespace Keyboard
