#include "MatchView.hpp"

#include "../Common/Navigator.hpp"
#include "../Common/Theme.hpp"
#include "../Common/Widgets.hpp"
#include "MatchFeature.hpp"

namespace MatchView {

namespace {

/// The screen's widgets. The columns are rebuilt once a round; a tap only
/// changes which cards are filled and which are put aside, so the rest of
/// the page stays as it is — an e-ink screen flashes on every redraw.
struct Board {
    MatchFeature& feature;
    GtkWidget* status;
    GtkWidget* columns;
    GtkWidget* next;
    std::vector<GtkWidget*> fronts;
    std::vector<GtkWidget*> backs;
    int round = 0;
};

GtkWidget* front(const Card& card) {
    std::string text = "<b>" + Widgets::escape(card.front) + "</b>";
    if (!card.lemma.empty()) text += "\n" + Widgets::small(Widgets::escape(card.lemma));
    GtkWidget* label = Widgets::markup(text, 0.5);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    return label;
}

GtkWidget* back(const Card& card) {
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(3));
    gtk_box_pack_start(GTK_BOX(box), Widgets::label(card.back), FALSE, FALSE, 0);
    if (!card.example.empty()) {
        gtk_box_pack_start(GTK_BOX(box), Widgets::markup(Widgets::small("<i>" + Widgets::escape(card.example) + "</i>")), FALSE, FALSE, 0);
    }
    return box;
}

void clear(GtkWidget* container) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(container));
    for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
    g_list_free(children);
}

void build(Board& board) {
    clear(board.columns);
    board.fronts.clear();
    board.backs.clear();
    board.round = board.feature.roundNumber();
    if (!board.feature.hasRound()) {
        gtk_box_pack_start(GTK_BOX(board.columns),
            Widgets::label("Look up two words or more while reading, then come back to pair them with their meanings."), TRUE, TRUE, 0);
        gtk_widget_show_all(board.columns);
        return;
    }

    const MatchRound& round = board.feature.round();
    MatchFeature& feature = board.feature;
    GtkWidget* fronts = gtk_vbox_new(TRUE, Widgets::px(10));
    GtkWidget* backs = gtk_vbox_new(TRUE, Widgets::px(10));
    for (int card : round.fronts()) {
        GtkWidget* button = Widgets::contentButton(front(round.cards()[card]), [&feature, card] { feature.pickFront(card); });
        board.fronts.push_back(button);
        gtk_box_pack_start(GTK_BOX(fronts), button, TRUE, TRUE, 0);
    }
    for (int card : round.backs()) {
        GtkWidget* button = Widgets::contentButton(back(round.cards()[card]), [&feature, card] { feature.pickBack(card); });
        board.backs.push_back(button);
        gtk_box_pack_start(GTK_BOX(backs), button, TRUE, TRUE, 0);
    }
    gtk_widget_set_size_request(fronts, Widgets::px(160), -1);
    gtk_box_pack_start(GTK_BOX(board.columns), fronts, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(board.columns), backs, TRUE, TRUE, 0);
    gtk_widget_show_all(board.columns);
}

std::string status(const MatchRound& round) {
    std::string misses = round.misses() == 0 ? "no misses"
        : std::to_string(round.misses()) + (round.misses() == 1 ? " miss" : " misses");
    if (round.isComplete()) return "All " + std::to_string(round.matchedCount()) + " paired, " + misses + ".";
    std::string progress = std::to_string(round.matchedCount()) + " of " + std::to_string(round.cards().size()) + " paired, " + misses + ".";
    if (!round.lastPick()) return round.matchedCount() == 0 ? "Tap a word, then the meaning it had." : progress;
    return (round.lastPick()->matched ? "✓ A pair. " : "✕ Not a pair. ") + progress;
}

void refresh(Board& board) {
    if (!board.feature.hasRound()) {
        gtk_widget_hide(board.status);
        gtk_widget_hide(board.next);
        return;
    }
    const MatchRound& round = board.feature.round();
    for (size_t i = 0; i < board.fronts.size(); ++i) {
        int card = round.fronts()[i];
        Theme::setSelected(board.fronts[i], round.selectedFront() == card);
        gtk_widget_set_sensitive(board.fronts[i], !round.isMatched(card));
    }
    for (size_t i = 0; i < board.backs.size(); ++i) {
        int card = round.backs()[i];
        Theme::setSelected(board.backs[i], round.selectedBack() == card);
        gtk_widget_set_sensitive(board.backs[i], !round.isMatched(card));
    }
    gtk_label_set_text(GTK_LABEL(board.status), status(round).c_str());
    gtk_widget_show(board.status);
    if (round.isComplete()) gtk_widget_show(board.next); else gtk_widget_hide(board.next);
}

}  // namespace

void open(Env& env, Navigator& navigator, long long bookId) {
    auto* feature = new MatchFeature(env, bookId);
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(12));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(12));

    auto* board = new Board{*feature, Widgets::label(""), gtk_hbox_new(FALSE, Widgets::px(10)),
                            Widgets::button("Next round", [feature] { feature->nextRound(); }), {}, {}};
    gtk_widget_set_size_request(board->next, -1, Widgets::px(48));
    // Shown and hidden by `refresh`, out of reach of the screen's show-all.
    gtk_widget_set_no_show_all(board->status, TRUE);
    gtk_widget_set_no_show_all(board->next, TRUE);
    gtk_box_pack_start(GTK_BOX(box), board->status, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), board->columns, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), board->next, FALSE, FALSE, 0);

    auto render = [board] {
        if (board->round != board->feature.roundNumber()) build(*board);
        refresh(*board);
    };
    feature->onChange = render;
    render();

    GtkWidget* screen = Widgets::screen("Practice", Widgets::scrolled(box), [&navigator] { navigator.pop(); }, "Done");
    Widgets::own(screen, feature);
    Widgets::own(screen, board);
    navigator.push(screen);
}

}  // namespace MatchView
