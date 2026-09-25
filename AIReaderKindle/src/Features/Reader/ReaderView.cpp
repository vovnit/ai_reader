#include "ReaderView.hpp"

#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "../Lookup/LookupView.hpp"
#include "../Menu/MenuView.hpp"
#include "../../Domain/Reading/Illustrations.hpp"
#include "../../Services/EinkDisplay.hpp"
#include "ReaderFeature.hpp"


namespace ReaderView {

namespace {

struct Screen {
    Env& env;
    Navigator& navigator;
    ReaderFeature feature;
    GtkWidget* area = nullptr;
    GtkWidget* progress = nullptr;
    /// The page on screen, drawn once; exposes copy it.
    cairo_surface_t* shown = nullptr;
    bool animates = true;
    /// Set around a page turn, so the page it brings slides in on an e-ink
    /// screen instead of just appearing.
    std::optional<EinkDisplay::Slide> turning;

    Screen(Env& env, Navigator& navigator, const Book& book)
        : env(env), navigator(navigator), feature(env, book) {}

    ~Screen() {
        if (shown) cairo_surface_destroy(shown);
    }

    void render() {
        std::string text;
        if (feature.status() == ReaderFeature::Status::Loaded && feature.pageCount() > 0) {
            text = "Chapter " + std::to_string(feature.chapterIndex() + 1) + "/" + std::to_string(feature.chapterCount())
                + "  ·  Page " + std::to_string(feature.pageIndex() + 1) + "/" + std::to_string(feature.pageCount());
        } else {
            text = feature.book().title;
        }
        gtk_button_set_label(GTK_BUTTON(progress), text.c_str());

        cairo_surface_t* next = renderPage();
        if (shown) cairo_surface_destroy(shown);
        shown = next;
        if (turning && next && area->window && slide(*turning)) {
            // The panel has the page, sliding in. X draws the same pixels
            // over it once the slide is over, so the two agree without the
            // second update getting in the way of the first.
            g_timeout_add(slideMilliseconds, [](gpointer widget) -> gboolean {
                gtk_widget_queue_draw(GTK_WIDGET(widget));
                g_object_unref(widget);
                return FALSE;
            }, g_object_ref(area));
        } else {
            gtk_widget_queue_draw(area);
        }
    }

    /// How long the panel's slide is given before X draws over it.
    static constexpr guint slideMilliseconds = 600;

    /// Puts the page on the e-ink panel itself, sliding in.
    bool slide(EinkDisplay::Slide direction) {
        int x = 0, y = 0;
        gdk_window_get_origin(area->window, &x, &y);
        return EinkDisplay::slide(shown, x, y, direction);
    }

    /// Runs `action`, sliding in the page it turns to.
    void turnPage(EinkDisplay::Slide direction, const std::function<void()>& action) {
        if (animates) turning = direction;
        action();
        turning.reset();
    }

    /// The page as a picture the area's size, or null when there is none.
    cairo_surface_t* renderPage() {
        const Page* page = feature.page();
        int width = area->allocation.width;
        int height = area->allocation.height;
        if (feature.status() != ReaderFeature::Status::Loaded || !page || !feature.layout().layout
            || width < 1 || height < 1) return nullptr;
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
        cairo_t* cr = cairo_create(surface);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        int margin = feature.marginPixels();
        // The whole chapter is laid out once; a page is the slice of it that
        // starts at the page's top.
        cairo_rectangle(cr, margin, margin, width - 2 * margin, page->height / PANGO_SCALE + 1);
        cairo_clip(cr);
        cairo_translate(cr, margin, margin - page->top / static_cast<double>(PANGO_SCALE));
        pango_cairo_show_layout(cr, feature.layout().layout);
        cairo_destroy(cr);
        return surface;
    }
};

void drawCentered(cairo_t* cr, GtkWidget* widget, const std::string& text) {
    PangoLayout* layout = gtk_widget_create_pango_layout(widget, text.c_str());
    pango_layout_set_width(layout, (widget->allocation.width - 40) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    int height = 0;
    pango_layout_get_pixel_size(layout, nullptr, &height);
    cairo_move_to(cr, 20, (widget->allocation.height - height) / 2.0);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

gboolean expose(GtkWidget* widget, GdkEventExpose* event, gpointer data) {
    auto* screen = static_cast<Screen*>(data);
    ReaderFeature& feature = screen->feature;
    cairo_t* cr = gdk_cairo_create(widget->window);
    // Only what was asked for, so the e-ink refreshes no more than that.
    gdk_cairo_region(cr, event->region);
    cairo_clip(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);

    if (screen->shown) {
        cairo_set_source_surface(cr, screen->shown, 0, 0);
        cairo_paint(cr);
    } else if (feature.status() == ReaderFeature::Status::Loading) {
        drawCentered(cr, widget, "Opening…");
    } else if (feature.status() == ReaderFeature::Status::Failed) {
        drawCentered(cr, widget, "Couldn’t open the book\n" + feature.error());
    }
    cairo_destroy(cr);
    return TRUE;
}

void sizeAllocate(GtkWidget* widget, GtkAllocation* allocation, gpointer data) {
    auto* screen = static_cast<Screen*>(data);
    PangoContext* context = gtk_widget_get_pango_context(widget);
    Illustrations::install(context);
    screen->feature.setContext(context, Widgets::scale());
    screen->feature.setPageSize(allocation->width, allocation->height);
}

gboolean pressed(GtkWidget*, GdkEventButton* event, gpointer data) {
    auto* screen = static_cast<Screen*>(data);
    if (event->type != GDK_BUTTON_PRESS) return TRUE;
    ReaderFeature::Tap tap;
    // A tap on blank space turns toward the side it is on: the page slides
    // that way.
    auto direction = event->x < screen->area->allocation.width / 2.0
        ? EinkDisplay::Slide::Right : EinkDisplay::Slide::Left;
    screen->turnPage(direction, [&] { tap = screen->feature.tapAt(event->x, event->y); });
    if (tap.kind == ReaderFeature::Tap::Kind::Word) {
        LookupContext context{
            tap.selection.word, tap.selection.sentence,
            screen->feature.language(), screen->feature.book().id,
        };
        LookupView::open(screen->env, screen->navigator, context, link(screen->env, screen->navigator, screen->feature));
    }
    return TRUE;
}

}  // namespace

void open(Env& env, Navigator& navigator, const Book& book) {
    auto* screen = new Screen(env, navigator, book);

    screen->area = gtk_drawing_area_new();
    screen->animates = env.settings.animatesTurns();
    gtk_widget_add_events(screen->area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(screen->area, "expose-event", G_CALLBACK(expose), screen);
    g_signal_connect(screen->area, "size-allocate", G_CALLBACK(sizeAllocate), screen);
    g_signal_connect(screen->area, "button-press-event", G_CALLBACK(pressed), screen);

    GtkWidget* footer = gtk_hbox_new(FALSE, Widgets::px(6));
    gtk_container_set_border_width(GTK_CONTAINER(footer), Widgets::px(4));
    GtkWidget* previous = Widgets::glyphButton("‹", [screen] {
        screen->turnPage(EinkDisplay::Slide::Right, [screen] { screen->feature.previous(); });
    });
    GtkWidget* next = Widgets::glyphButton("›", [screen] {
        screen->turnPage(EinkDisplay::Slide::Left, [screen] { screen->feature.next(); });
    });
    gtk_widget_set_size_request(previous, Widgets::px(56), -1);
    gtk_widget_set_size_request(next, Widgets::px(56), -1);
    screen->progress = Widgets::button(book.title, [screen] {
        MenuView::open(screen->env, screen->navigator, screen->feature, link(screen->env, screen->navigator, screen->feature));
    });
    gtk_button_set_relief(GTK_BUTTON(screen->progress), GTK_RELIEF_NONE);
    gtk_box_pack_start(GTK_BOX(footer), previous, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(footer), screen->progress, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(footer), next, FALSE, FALSE, 0);

    GtkWidget* box = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), screen->area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), footer, FALSE, FALSE, 0);
    Widgets::own(box, screen);
    Navigator::tag(box, "reader");

    screen->feature.onChange = [screen] { screen->render(); };
    // Display changes made from the menu land here when the reader comes back.
    Navigator::onReturn(box, [screen] {
        screen->animates = screen->env.settings.animatesTurns();
        screen->feature.setStyle(screen->env.settings.style());
    });

    navigator.push(box);
    screen->feature.load();
}

ReaderLink link(Env& env, Navigator& navigator, ReaderFeature& reader) {
    return {reader.scope(), [&env, &navigator, &reader](const BookPosition& position) {
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
        // The pop runs from the main loop; the push must queue behind it.
        Widgets::later([&env, &navigator, book = *other] { open(env, navigator, book); });
    }};
}

}  // namespace ReaderView
