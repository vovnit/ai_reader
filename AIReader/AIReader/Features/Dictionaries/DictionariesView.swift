import ComposableArchitecture
import SwiftUI
import UniformTypeIdentifiers

struct DictionariesView: View {
    @Bindable var store: StoreOf<DictionariesFeature>

    var body: some View {
        List {
            Section {
                ForEach(store.packs) { pack in
                    row(pack)
                }
                if store.isImporting {
                    HStack(spacing: 10) {
                        ProgressView()
                        Text("Converting…").foregroundStyle(.secondary)
                    }
                }
            } footer: {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Add a dictionary as an AIReader pack, StarDict, XDXF, Lingvo DSL, or a tab- or comma-separated word list. StarDict needs its .ifo, .idx and .dict files picked together. Disabled dictionaries are not searched.")
                    // The bundled dictionary's sources ask to be named, and their license with them.
                    Text(verbatim: "The dictionary that ships with the app is selected and converted from Lexique 4, by Boris New, Christophe Pallier and others (lexique.org), and from the Russian Wiktionary, by its contributors (ru.wiktionary.org, by way of kaikki.org). Both are licensed CC BY-SA 4.0 (creativecommons.org/licenses/by-sa/4.0), and so is this dictionary.")
                }
            }
        }
        .navigationTitle("Dictionaries")
        .toolbar {
            Button("Add dictionary", systemImage: "plus") { store.send(.addTapped) }
        }
        .fileImporter(
            isPresented: $store.isImporterPresented,
            allowedContentTypes: [.database, .data],
            allowsMultipleSelection: true
        ) { store.send(.filesPicked($0)) }
        .alert($store.scope(state: \.alert, action: \.alert))
    }

    private func row(_ pack: DictionaryPack) -> some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(pack.name)
                if let languages = pack.languages {
                    Text(languages)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            Spacer()
            Toggle("Enabled", isOn: .constant(pack.isEnabled))
                .labelsHidden()
                .onTapGesture { store.send(.toggled(pack)) }
        }
        .swipeActions {
            if !pack.isBundled {
                Button("Delete", systemImage: "trash", role: .destructive) {
                    store.send(.deleteTapped(pack))
                }
            }
        }
    }
}
