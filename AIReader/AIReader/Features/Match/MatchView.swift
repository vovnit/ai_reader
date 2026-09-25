import ComposableArchitecture
import SwiftUI

/// The board: words down one column, meanings down the other. A tap fills a
/// card until its pair is chosen; a matched pair fades out.
struct MatchView: View {
    let store: StoreOf<MatchFeature>

    var body: some View {
        Group {
            if let round = store.round {
                board(round)
            } else if store.isLoaded {
                ContentUnavailableView(
                    "Nothing to practise yet",
                    systemImage: "rectangle.on.rectangle.angled",
                    description: Text("Look up two words or more while reading, then come back to pair them with their meanings.")
                )
            } else {
                ProgressView()
            }
        }
        .navigationTitle("Practice")
        .task { await store.send(.task).finish() }
    }

    private func board(_ round: MatchRound) -> some View {
        ScrollView {
            VStack(spacing: 16) {
                Text(status(round))
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, alignment: .leading)

                HStack(alignment: .top, spacing: 10) {
                    VStack(spacing: 10) {
                        ForEach(round.fronts, id: \.self) { card in
                            tile(selected: round.selectedFront == card, matched: round.isMatched(card)) {
                                store.send(.frontTapped(card))
                            } label: {
                                front(round.cards[card])
                            }
                        }
                    }
                    .frame(width: 130)

                    VStack(spacing: 10) {
                        ForEach(round.backs, id: \.self) { card in
                            tile(selected: round.selectedBack == card, matched: round.isMatched(card)) {
                                store.send(.backTapped(card))
                            } label: {
                                back(round.cards[card])
                            }
                        }
                    }
                }

                if round.isComplete {
                    Button("Next round") { store.send(.nextRoundTapped) }
                        .buttonStyle(.borderedProminent)
                }
            }
            .padding()
        }
    }

    private func tile(
        selected: Bool,
        matched: Bool,
        action: @escaping () -> Void,
        @ViewBuilder label: () -> some View
    ) -> some View {
        Button(action: action) {
            label()
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(10)
                .background(selected ? Color.accentColor.opacity(0.2) : Color.secondary.opacity(0.15))
                .clipShape(RoundedRectangle(cornerRadius: 10))
                .overlay {
                    RoundedRectangle(cornerRadius: 10)
                        .strokeBorder(selected ? Color.accentColor : .clear, lineWidth: 2)
                }
        }
        .buttonStyle(.plain)
        .disabled(matched)
        .opacity(matched ? 0.25 : 1)
        .animation(.default, value: matched)
    }

    private func front(_ card: Card) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(card.front).font(.headline)
            if let lemma = card.lemma {
                Text(lemma).font(.caption).foregroundStyle(.secondary)
            }
        }
    }

    private func back(_ card: Card) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(card.back).font(.subheadline)
            if !card.example.isEmpty {
                Text(card.example).font(.caption).italic().foregroundStyle(.secondary)
            }
        }
    }

    private func status(_ round: MatchRound) -> String {
        let misses = round.misses == 0 ? "no misses" : "\(round.misses) miss\(round.misses == 1 ? "" : "es")"
        if round.isComplete { return "All \(round.matchedCount) paired, \(misses)." }
        let progress = "\(round.matchedCount) of \(round.cards.count) paired, \(misses)."
        guard let pick = round.lastPick else {
            return round.matchedCount == 0 ? "Tap a word, then the meaning it had." : progress
        }
        return (pick.matched ? "✓ A pair. " : "✕ Not a pair. ") + progress
    }
}
