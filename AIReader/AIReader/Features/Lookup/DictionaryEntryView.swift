import ComposableArchitecture
import SwiftUI

/// The article a lookup was drawn from, as the dictionary has it: every
/// sense, and which dictionary it came from.
@Reducer
struct DictionaryEntryFeature {
    @ObservableState
    struct State: Equatable {
        let lemma: String
        let articles: [DictionaryLookup.Article]
    }

    enum Action: Equatable {}

    var body: some ReducerOf<Self> { EmptyReducer() }
}

struct DictionaryEntryView: View {
    let store: StoreOf<DictionaryEntryFeature>

    var body: some View {
        List {
            ForEach(Array(store.articles.enumerated()), id: \.offset) { _, article in
                Section {
                    ForEach(Array(article.senses.enumerated()), id: \.offset) { index, sense in
                        Text("\(index + 1).  \(sense)")
                    }
                } header: {
                    HStack(alignment: .firstTextBaseline) {
                        Text(article.lemma).font(.headline)
                        if let partOfSpeech = article.partOfSpeech {
                            Text(partOfSpeech).font(.caption)
                        }
                    }
                    .textCase(nil)
                } footer: {
                    if let source = article.source { Text(source) }
                }
            }
        }
        .navigationTitle(store.lemma)
    }
}
