import ComposableArchitecture
import SwiftUI

struct SettingsView: View {
    @Bindable var store: StoreOf<SettingsFeature>

    var body: some View {
        NavigationStack {
            Form {
                Section("Service") {
                    TextField("Endpoint", text: $store.settings.endpoint)
                        .textContentType(.URL)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        .keyboardType(.URL)
                        #endif
                    SecureField("Token", text: $store.settings.apiKey)
                }

                Section("Model") {
                    if store.selectableModels.isEmpty {
                        TextField("Model", text: $store.settings.model)
                            .autocorrectionDisabled()
                            #if os(iOS)
                            .textInputAutocapitalization(.never)
                            #endif
                    } else {
                        Picker("Model", selection: $store.settings.model) {
                            ForEach(store.selectableModels, id: \.self) { model in
                                Text(model).tag(model)
                            }
                        }
                    }

                    Button {
                        store.send(.loadModelsTapped)
                    } label: {
                        if store.isLoadingModels {
                            ProgressView()
                        } else {
                            Text("Load models")
                        }
                    }
                    .disabled(store.isLoadingModels)
                }

                Section {
                    TextField("Language", text: $store.settings.language)
                        .autocorrectionDisabled()
                } header: {
                    Text("Explain in")
                } footer: {
                    Text("The language of explanations and answers, such as English.")
                }

                Section {
                    Button("Dictionaries") { store.send(.dictionariesTapped) }
                }

                Section {
                    SecureField("Monid token", text: $store.web.apiKey)
                    TextField("Provider", text: $store.web.provider)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        #endif
                    TextField("Endpoint", text: $store.web.endpoint)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        #endif
                    TextField("Input", text: $store.web.input, axis: .vertical)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        #endif
                } header: {
                    Text("Web search")
                } footer: {
                    Text("With a Monid token the model can search the web for a name, a place or an expression the dictionary and the book do not explain. The provider and endpoint are as “monid discover” lists them; the input is what the endpoint is sent, with $query for the words searched and $language for the book's language. Some endpoints are free, most are paid per call.")
                }

                Section {
                    TextField("WebDAV folder", text: $store.sync.url)
                        .textContentType(.URL)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        .keyboardType(.URL)
                        #endif
                    TextField("User name", text: $store.sync.username)
                        .textContentType(.username)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        #endif
                    SecureField("Password", text: $store.sync.password)
                    Button {
                        store.send(.syncTapped)
                    } label: {
                        if store.isSyncing {
                            ProgressView()
                        } else {
                            Text("Sync now")
                        }
                    }
                    .disabled(store.isSyncing || !store.sync.isConfigured)
                } header: {
                    Text("Sync")
                } footer: {
                    Text(store.syncMessage ?? "Books go to its Books folder, and reading positions, groups and looked-up words to one file beside it, shared with the Kindle app. Syncs when the app opens and when a book is closed.")
                }

                if let message = store.errorMessage {
                    Section {
                        Text(message)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle("Settings")
            .navigationDestination(
                item: $store.scope(state: \.dictionaries, action: \.dictionaries)
            ) { dictionaries in
                DictionariesView(store: dictionaries)
            }
            .toolbar {
                Button("Done") { store.send(.doneTapped) }
            }
        }
    }
}
