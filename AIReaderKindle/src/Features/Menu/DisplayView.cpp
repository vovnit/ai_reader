#include "DisplayView.hpp"

#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "../Reader/ReaderFeature.hpp"
#include "DisplayFeature.hpp"

#include <cstdio>

namespace DisplayView {

namespace {

/// A setting as `name  –  value  +`.
GtkWidget* row(GtkWidget* box, const std::string& name, Widgets::Action down, Widgets::Action up) {
    GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(8));
    GtkWidget* title = Widgets::label(name, 0, false);
    gtk_misc_set_alignment(GTK_MISC(title), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(line), title, TRUE, TRUE, 0);
    GtkWidget* minus = Widgets::glyphButton("–", std::move(down));
    GtkWidget* value = Widgets::label("", 0.5, false);
    gtk_misc_set_alignment(GTK_MISC(value), 0.5, 0.5);
    gtk_widget_set_size_request(value, Widgets::px(64), -1);
    GtkWidget* plus = Widgets::glyphButton("+", std::move(up));
    gtk_widget_set_size_request(minus, Widgets::px(56), Widgets::px(44));
    gtk_widget_set_size_request(plus, Widgets::px(56), Widgets::px(44));
    gtk_box_pack_start(GTK_BOX(line), minus, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(line), value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(line), plus, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), line, FALSE, FALSE, 0);
    return value;
}

std::string number(double value, int decimals) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, decimals ? "%.1f" : "%.0f", value);
    return buffer;
}

}  // namespace

void open(Env& env, Navigator& navigator, ReaderFeature& reader) {
    auto* feature = new DisplayFeature(env, [&reader](const ReadingStyle& style) { reader.setStyle(style); });

    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(12));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(16));

    GtkWidget* scale = row(box, "Size", [feature] { feature->adjustScale(-1); }, [feature] { feature->adjustScale(1); });
    GtkWidget* spacing = row(box, "Line spacing", [feature] { feature->adjustLineSpacing(-1); }, [feature] { feature->adjustLineSpacing(1); });
    GtkWidget* margin = row(box, "Margins", [feature] { feature->adjustMargin(-1); }, [feature] { feature->adjustMargin(1); });

    GtkWidget* faceLine = gtk_hbox_new(FALSE, Widgets::px(8));
    GtkWidget* faceTitle = Widgets::label("Face", 0, false);
    gtk_misc_set_alignment(GTK_MISC(faceTitle), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(faceLine), faceTitle, TRUE, TRUE, 0);
    GtkWidget* faces = Widgets::button("", [feature, &navigator] {
        std::string current = feature->style().fontName.empty() ? "Serif" : feature->style().fontName;
        Widgets::picker(navigator, "Face", ReadingStyle::fontNames(), current,
                        [feature](const std::string& name) { feature->setFont(name); });
    });
    gtk_widget_set_size_request(faces, Widgets::px(180), Widgets::px(44));
    gtk_box_pack_start(GTK_BOX(faceLine), faces, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), faceLine, FALSE, FALSE, 0);

    GtkWidget* turnLine = gtk_hbox_new(FALSE, Widgets::px(8));
    GtkWidget* turnTitle = Widgets::label("Page turn animation", 0, false);
    gtk_misc_set_alignment(GTK_MISC(turnTitle), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(turnLine), turnTitle, TRUE, TRUE, 0);
    GtkWidget* turns = Widgets::check(feature->animatesTurns(), [feature](bool on) { feature->setAnimatesTurns(on); });
    gtk_box_pack_start(GTK_BOX(turnLine), turns, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), turnLine, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Widgets::separator(), FALSE, FALSE, 0);
    GtkWidget* preview = Widgets::markup("");
    gtk_box_pack_start(GTK_BOX(box), preview, FALSE, FALSE, 0);

    auto render = [feature, scale, spacing, margin, faces, preview] {
        const ReadingStyle& style = feature->style();
        gtk_label_set_text(GTK_LABEL(scale), number(style.scale, 1).c_str());
        gtk_label_set_text(GTK_LABEL(spacing), number(style.lineSpacing, 0).c_str());
        gtk_label_set_text(GTK_LABEL(margin), number(style.margin, 0).c_str());
        std::string family = style.fontName.empty() ? "Serif" : style.fontName;
        gtk_button_set_label(GTK_BUTTON(faces), family.c_str());
        std::string size = number(ReadingStyle::basePixelSize * style.scale * Widgets::scale(), 0);
        gtk_label_set_markup(GTK_LABEL(preview),
            ("<span font_desc=\"" + Widgets::escape(family) + " " + size + "px\">"
             "The quick brown fox jumps over the lazy dog.</span>").c_str());
    };
    feature->onChange = render;
    render();

    GtkWidget* screen = Widgets::screen("Display", box, [&navigator] { navigator.pop(); });
    Widgets::own(screen, feature);
    navigator.push(screen);
}

}  // namespace DisplayView
