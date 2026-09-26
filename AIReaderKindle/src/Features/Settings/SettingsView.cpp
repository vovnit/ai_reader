#include "SettingsView.hpp"

#include "../Common/Keyboard.hpp"
#include "../Common/Navigator.hpp"
#include "../../Services/Paths.hpp"
#include "../Common/Widgets.hpp"
#include "../Dictionaries/DictionariesView.hpp"
#include "SettingsFeature.hpp"

namespace SettingsView {

namespace {

struct Screen {
    SettingsFeature feature;
    Navigator& navigator;
    GtkWidget* modelHolder = nullptr;
    GtkWidget* modelEntry = nullptr;
    GtkWidget* load = nullptr;
    GtkWidget* status = nullptr;
    GtkWidget* syncButton = nullptr;
    GtkWidget* syncStatus = nullptr;

    Screen(Env& env, Navigator& navigator) : feature(env), navigator(navigator) {}

    /// A free-text field until the endpoint has listed its models, then a
    /// button that opens the list.
    void rebuildModelPicker() {
        GList* children = gtk_container_get_children(GTK_CONTAINER(modelHolder));
        for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
        g_list_free(children);

        if (feature.models().empty()) {
            modelEntry = Widgets::entry(feature.settings().model);
            Widgets::connect(modelEntry, "changed", [this] { feature.setModel(Widgets::entryText(modelEntry)); });
            gtk_box_pack_start(GTK_BOX(modelHolder), modelEntry, TRUE, TRUE, 0);
        } else {
            modelEntry = nullptr;
            GtkWidget* picker = Widgets::button(feature.settings().model, [this] {
                Widgets::picker(navigator, "Model", feature.models(), feature.settings().model,
                                [this](const std::string& model) { feature.setModel(model); });
            });
            gtk_misc_set_alignment(GTK_MISC(gtk_bin_get_child(GTK_BIN(picker))), 0, 0.5);
            gtk_box_pack_start(GTK_BOX(modelHolder), picker, TRUE, TRUE, 0);
        }
        gtk_widget_show_all(modelHolder);
    }

    void render() {
        gtk_button_set_label(GTK_BUTTON(load), feature.isLoadingModels() ? "Loading…" : "Load models");
        gtk_widget_set_sensitive(load, !feature.isLoadingModels());
        gtk_label_set_text(GTK_LABEL(status), feature.error().c_str());
        // The picker button shows whichever model was chosen.
        if (!modelEntry && !feature.models().empty()) rebuildModelPicker();
        gtk_button_set_label(GTK_BUTTON(syncButton), feature.isSyncing() ? "Syncing…" : "Sync now");
        gtk_widget_set_sensitive(syncButton, !feature.isSyncing() && feature.sync().isConfigured());
        gtk_label_set_text(GTK_LABEL(syncStatus), feature.syncMessage().c_str());
    }
};

GtkWidget* field(GtkWidget* box, const std::string& name, GtkWidget* widget) {
    gtk_box_pack_start(GTK_BOX(box), Widgets::markup(Widgets::small(Widgets::escape(name)), 0, false), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0);
    return widget;
}

}  // namespace

void open(Env& env, Navigator& navigator) {
    auto* screen = new Screen(env, navigator);
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(8));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(16));

    GtkWidget* endpoint = field(box, "Endpoint", Widgets::entry(screen->feature.settings().endpoint));
    Widgets::connect(endpoint, "changed", [screen, endpoint] { screen->feature.setEndpoint(Widgets::entryText(endpoint)); });
    GtkWidget* token = field(box, "Token", Widgets::entry(screen->feature.settings().apiKey, true));
    Widgets::connect(token, "changed", [screen, token] { screen->feature.setToken(Widgets::entryText(token)); });
    GtkWidget* openAI = field(box, "OpenAI token (used when the endpoint is api.openai.com)",
                              Widgets::entry(screen->feature.settings().openAIKey, true));
    Widgets::connect(openAI, "changed", [screen, openAI] { screen->feature.setOpenAIToken(Widgets::entryText(openAI)); });

    screen->modelHolder = gtk_hbox_new(FALSE, 0);
    field(box, "Model", screen->modelHolder);
    screen->rebuildModelPicker();
    screen->load = Widgets::button("Load models", [screen] { screen->feature.loadModels(); });
    gtk_box_pack_start(GTK_BOX(box), screen->load, FALSE, FALSE, 0);
    screen->status = Widgets::label("");
    gtk_box_pack_start(GTK_BOX(box), screen->status, FALSE, FALSE, 0);
    GtkWidget* language = field(box, "Explain in (the language of explanations and answers, such as English)",
                                Widgets::entry(screen->feature.settings().language));
    Widgets::connect(language, "changed", [screen, language] { screen->feature.setLanguage(Widgets::entryText(language)); });

    gtk_box_pack_start(GTK_BOX(box), Widgets::separator(), FALSE, FALSE, Widgets::px(6));
    gtk_box_pack_start(GTK_BOX(box), Widgets::button("Dictionaries", [&env, &navigator] {
        DictionariesView::open(env, navigator);
    }), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Widgets::separator(), FALSE, FALSE, Widgets::px(6));
    GtkWidget* webToken = field(box, "Monid token (lets the model search the web; some endpoints are free, most are paid per call)",
                                Widgets::entry(screen->feature.webSearch().apiKey, true));
    Widgets::connect(webToken, "changed", [screen, webToken] { screen->feature.setWebToken(Widgets::entryText(webToken)); });
    GtkWidget* webProvider = field(box, "Search provider", Widgets::entry(screen->feature.webSearch().provider));
    Widgets::connect(webProvider, "changed", [screen, webProvider] { screen->feature.setWebProvider(Widgets::entryText(webProvider)); });
    GtkWidget* webEndpoint = field(box, "Search endpoint", Widgets::entry(screen->feature.webSearch().endpoint));
    Widgets::connect(webEndpoint, "changed", [screen, webEndpoint] { screen->feature.setWebEndpoint(Widgets::entryText(webEndpoint)); });
    GtkWidget* webInput = field(box, "Search input, as monid inspect lists it ($query: the words searched, $language: the book's)",
                                Widgets::entry(screen->feature.webSearch().input));
    Widgets::connect(webInput, "changed", [screen, webInput] { screen->feature.setWebInput(Widgets::entryText(webInput)); });

    gtk_box_pack_start(GTK_BOX(box), Widgets::separator(), FALSE, FALSE, Widgets::px(6));
    GtkWidget* syncUrl = field(box, "WebDAV folder (books, reading places, groups and words are shared with the iOS app)",
                               Widgets::entry(screen->feature.sync().url));
    Widgets::connect(syncUrl, "changed", [screen, syncUrl] { screen->feature.setSyncUrl(Widgets::entryText(syncUrl)); });
    GtkWidget* syncUser = field(box, "User name", Widgets::entry(screen->feature.sync().username));
    Widgets::connect(syncUser, "changed", [screen, syncUser] { screen->feature.setSyncUser(Widgets::entryText(syncUser)); });
    GtkWidget* syncPassword = field(box, "Password", Widgets::entry(screen->feature.sync().password, true));
    Widgets::connect(syncPassword, "changed", [screen, syncPassword] { screen->feature.setSyncPassword(Widgets::entryText(syncPassword)); });
    screen->syncButton = Widgets::button("Sync now", [screen] { screen->feature.syncNow(); });
    gtk_box_pack_start(GTK_BOX(box), screen->syncButton, FALSE, FALSE, 0);
    screen->syncStatus = Widgets::label("");
    gtk_box_pack_start(GTK_BOX(box), screen->syncStatus, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Widgets::separator(), FALSE, FALSE, Widgets::px(6));
    gtk_box_pack_start(GTK_BOX(box), Widgets::markup(Widgets::small(
        "Settings are kept in " + Widgets::escape(Paths::settings()) + ", which can also be edited "
        "from a computer. Set the endpoint to <tt>mock://ai</tt> to answer lookups without the network."
        "\n\nAIReader " AIREADER_VERSION)), FALSE, FALSE, 0);

    screen->feature.onChange = [screen] { screen->render(); };
    screen->feature.onModelsChange = [screen] { screen->rebuildModelPicker(); };
    screen->render();

    GtkWidget* body = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::scrolled(box), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Keyboard::create(body), FALSE, FALSE, 0);
    GtkWidget* widget = Widgets::screen("Settings", body, [&navigator] { navigator.pop(); }, "Done");
    Widgets::own(widget, screen);
    navigator.push(widget);
}

}  // namespace SettingsView
