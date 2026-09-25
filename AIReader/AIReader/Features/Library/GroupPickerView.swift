import ComposableArchitecture
import SwiftUI

struct GroupPickerView: View {
    @Bindable var store: StoreOf<GroupPickerFeature>

    var body: some View {
        NavigationStack {
            List {
                Section {
                    row("No group", selected: store.book.groupID == nil) { store.send(.groupTapped(nil)) }
                    ForEach(store.groups) { group in
                        row(group.name, selected: store.book.groupID == group.id) {
                            store.send(.groupTapped(group.id))
                        }
                    }
                } footer: {
                    Text("A series, an author, a course: books in one group are searched together.")
                }

                Section("New group") {
                    TextField("Name", text: $store.newName)
                        .onSubmit { store.send(.createTapped) }
                    Button("Create") { store.send(.createTapped) }
                        .disabled(store.newName.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
            .navigationTitle(store.book.title)
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                Button("Cancel") { store.send(.cancelTapped) }
            }
        }
        .presentationDetents([.medium, .large])
    }

    private func row(_ name: String, selected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack {
                Text(name).foregroundStyle(.primary)
                Spacer()
                if selected { Image(systemName: "checkmark").foregroundStyle(.tint) }
            }
        }
    }
}
