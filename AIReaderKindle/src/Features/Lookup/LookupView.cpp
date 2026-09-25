#include "LookupView.hpp"

#include "../../Domain/AI/ChatPrompt.hpp"
#include "../Chat/ChatView.hpp"
#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "../XRay/XRayView.hpp"
#include "EntryView.hpp"
#include "LookupFeature.hpp"

namespace LookupView {

namespace {

GtkWidget* rendered(Env& env, const LookupFeature& feature, Navigator& navigator, const ReaderLink& link) {
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(14));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(12));
    auto add = [&](GtkWidget* widget) { gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0); };
    const LookupContext& context = feature.context();

    if (const auto& explanation = feature.explanation()) {
        add(Widgets::markup(Widgets::big(Widgets::escape(explanation->meaning))));
        add(Widgets::markup("<b>" + Widgets::escape(explanation->lemma) + "</b>\n" +
                            Widgets::small(Widgets::escape(explanation->formNote))));

        GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(8));
        // Only when the dictionary really has the lemma; a guess has no entry.
        if (!feature.entry().empty()) {
            std::string lemma = explanation->lemma;
            auto articles = feature.entry();
            gtk_box_pack_start(GTK_BOX(line), Widgets::button("Dictionary entry", [&navigator, lemma, articles] {
                EntryView::open(navigator, lemma, articles);
            }), FALSE, FALSE, 0);
        }
        // What the book, rather than the dictionary, says the word is.
        if (link.scope.corpus) {
            std::string word = context.word;
            gtk_box_pack_start(GTK_BOX(line), Widgets::button("X-ray", [&env, &navigator, word, link] {
                XRayView::open(env, navigator, word, link);
            }), FALSE, FALSE, 0);
        }
        std::string seed = ChatPrompt::wordContext(context.word, context.sentence, *explanation);
        ReadingScope scope = feature.scope();
        gtk_box_pack_start(GTK_BOX(line), Widgets::button("Ask AI", [&env, &navigator, seed, scope] {
            ChatView::open(env, navigator, {seed, "Ask about this word — another example, a nuance, how it differs from a similar one.", scope});
        }), FALSE, FALSE, 0);
        add(line);

        if (!context.sentence.empty()) {
            add(Widgets::separator());
            add(Widgets::markup("<i>" + Widgets::escape(context.sentence) + "</i>"));
        }
        if (explanation->guessed || explanation->confidence < 0.6) {
            add(Widgets::separator());
            std::string note = explanation->guessed ? "Догадка, не из словаря\n" : "";
            note += "Уверенность: " + std::to_string(static_cast<int>(explanation->confidence * 100 + 0.5)) + "%";
            add(Widgets::markup(Widgets::small(Widgets::escape(note))));
        }
    } else if (!feature.error().empty()) {
        add(Widgets::label(feature.error()));
    } else {
        add(Widgets::label("Looking up…"));
        if (!context.sentence.empty()) add(Widgets::markup("<i>" + Widgets::escape(context.sentence) + "</i>"));
    }
    gtk_widget_show_all(box);
    return box;
}

}  // namespace

void open(Env& env, Navigator& navigator, const LookupContext& context, const ReaderLink& link) {
    auto* feature = new LookupFeature(env, context, link.scope);
    GtkWidget* body = Widgets::scrolled(rendered(env, *feature, navigator, link));
    GtkWidget* screen = Widgets::screen(context.word, body, [&navigator] { navigator.pop(); });
    Widgets::own(screen, feature);

    feature->onChange = [&env, feature, body, &navigator, link] {
        GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(body));
        gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(viewport)));
        gtk_container_add(GTK_CONTAINER(viewport), rendered(env, *feature, navigator, link));
    };

    navigator.push(screen);
    feature->start();
}

}  // namespace LookupView
