#include "../Features/Common/Theme.hpp"
#include "../Features/Common/Widgets.hpp"
#include "../Features/Library/LibraryView.hpp"
#include "../Services/ChatApi.hpp"
#include "../Services/Database.hpp"
#include "../Services/Env.hpp"
#include "../Services/Migrations.hpp"
#include "../Services/Paths.hpp"
#include "../Support/Files.hpp"
#include "../Features/Common/Navigator.hpp"
#include "SmokeScript.hpp"

#include <gtk/gtk.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/// `--endpoint` and `--model` override the saved settings for this run only,
/// which is how a test run is pointed at `mock://ai`.
static void readArguments(int argc, char** argv, std::string& endpoint, std::string& model) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--endpoint") == 0) endpoint = argv[++i];
        else if (std::strcmp(argv[i], "--model") == 0) model = argv[++i];
    }
}

int main(int argc, char** argv) {
    // Before anything else on the Kindle's GLib 2.29, which needs telling
    // that worker threads will post to the main loop.
    g_thread_init(nullptr);
    gtk_init(&argc, &argv);
    ChatApi::initialize();
    Paths::prepare();

    Database database(Paths::database());
    if (!Migrations::migrate(database)) {
        std::fprintf(stderr, "aireader: could not open %s: %s\n", Paths::database().c_str(), database.lastError().c_str());
        return 1;
    }
    Env env(database, Paths::settings());
    // Written with its defaults straight away, so it can be edited from a
    // computer before anything has been changed on the device.
    if (!Files::exists(Paths::settings())) {
        env.settings.saveAi(env.settings.ai());
        env.settings.saveStyle(env.settings.style());
    }
    std::string endpoint, model;
    readArguments(argc, argv, endpoint, model);
    env.settings.overrideForRun(endpoint, model);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    // The Kindle's window manager places and sizes the window by its title;
    // asking for the whole screen matches what `PC:N` gives.
    Widgets::kindleTitle(GTK_WINDOW(window), "A", "application", "_PC:N");
    GdkScreen* screen = gtk_widget_get_screen(window);
    int width = Paths::onKindle() ? gdk_screen_get_width(screen) : 600;
    int height = Paths::onKindle() ? gdk_screen_get_height(screen) : 800;
    // A desktop window the size of a Kindle screen, for looking at layouts.
    if (const char* size = g_getenv("AIREADER_WINDOW_SIZE")) std::sscanf(size, "%dx%d", &width, &height);
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    if (Paths::onKindle()) gtk_window_move(GTK_WINDOW(window), 0, 0);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    // Sizes are laid out for a 600-pixel-wide desktop window and scaled to
    // the screen; a Kindle is about twice as dense. `AIREADER_UI_SCALE` and
    // `AIREADER_UI_FONT` override the guess.
    double scale = std::max(1.0, width / 600.0);
    if (const char* forced = g_getenv("AIREADER_UI_SCALE")) scale = std::max(0.5, std::atof(forced));
    Widgets::setScale(scale);
    Widgets::setWrapWidth(width - Widgets::px(48));
    const char* font = g_getenv("AIREADER_UI_FONT");
    Theme::apply(font ? font : "Sans " + std::to_string(Widgets::fontPixels()) + "px");

    Navigator navigator;
    gtk_container_add(GTK_CONTAINER(window), navigator.widget());
    LibraryView::open(env, navigator);
    gtk_widget_show_all(window);
    SmokeScript::start(window);

    gtk_main();
    return 0;
}
