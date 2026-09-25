import ComposableArchitecture
import SwiftUI
import UniformTypeIdentifiers

/// The current subtitle line from the player, tappable word by word. Until
/// the player answers, the form for reaching it.
struct SubtitlesView: View {
    static let windowID = "subtitles"

    @Bindable var store: StoreOf<SubtitlesFeature>

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            if store.status == nil || store.isEditingConnection {
                connection
            } else if store.track == nil {
                Text(store.video.map { "No subtitles found beside \($0.lastPathComponent)." } ?? "Open a video in \(store.settings.kind.title).")
                    .foregroundStyle(.secondary)
                chooser("Choose subtitles…")
                if let message = store.errorMessage {
                    Text(message).font(.footnote).foregroundStyle(.secondary)
                }
                Spacer()
                footer
            } else {
                line
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .padding()
        .fileImporter(isPresented: $store.isImporterPresented, allowedContentTypes: subtitleTypes) {
            store.send(.subtitlesPicked($0))
        }
        .sheet(item: $store.scope(state: \.$lookup, action: \.lookup)) { lookup in
            LookupView(store: lookup)
        }
        .sheet(item: $store.scope(state: \.$picker, action: \.picker)) { picker in
            WebDAVPickerView(store: picker)
        }
        .task { await store.send(.task).finish() }
    }

    private var line: some View {
        VStack(alignment: .leading, spacing: 12) {
            if store.chunks.isEmpty {
                Text("…").foregroundStyle(.tertiary)
            } else {
                WordFlowView(chunks: store.chunks) { store.send(.wordTapped(utf16Offset: $0)) }
                    .font(.title2)
            }
            Spacer(minLength: 0)
            footer
        }
    }

    /// Which file the words come from, and the way to change it or the player.
    private var footer: some View {
        HStack {
            Text(store.subtitles?.lastPathComponent ?? "")
                .lineLimit(1)
            Spacer()
            if store.track != nil {
                chooser("Change…")
            }
            Button(store.settings.kind.title, systemImage: "gearshape") { store.send(.editConnectionTapped) }
        }
        .buttonStyle(.plain)
        .font(.footnote)
        .foregroundStyle(.secondary)
    }

    /// A subtitle file picked by hand, from the device or from the server.
    private func chooser(_ title: String) -> some View {
        Menu(title) {
            Button("On this device…", systemImage: "folder") { store.send(.chooseSubtitlesTapped(.device)) }
            Button("On the server…", systemImage: "externaldrive.connected.to.line.below") {
                store.send(.chooseSubtitlesTapped(.server))
            }
        }
    }

    private var connection: some View {
        Form {
            Picker("Player", selection: $store.settings.kind) {
                ForEach(PlayerSettings.Kind.allCases, id: \.self) { Text($0.title) }
            }
            .pickerStyle(.segmented)
            switch store.settings.kind {
            case .vlc:
                Text("In VLC, turn on Preferences → Interface → Main interfaces → Web, set a password, and open a video.")
                    .foregroundStyle(.secondary)
            case .kodi:
                Text("In Kodi, turn on Settings → Services → Control → Allow remote control via HTTP, set a password, and open a video.")
                    .foregroundStyle(.secondary)
                HStack {
                    TextField("Address", text: $store.settings.host)
                    if store.isFindingKodi {
                        ProgressView()
                    } else {
                        Button("Find") { store.send(.findKodiTapped) }
                    }
                }
                TextField("User name (kodi)", text: $store.settings.username)
            }
            TextField("Port", text: $store.settings.port)
            SecureField("Password", text: $store.settings.password)
            if let message = store.errorMessage {
                Text(message).font(.footnote).foregroundStyle(.secondary)
            }
            if store.status != nil {
                Button("Done") { store.send(.editConnectionTapped) }
            }
        }
        .formStyle(.grouped)
        .autocorrectionDisabled()
        #if os(iOS)
        .textInputAutocapitalization(.never)
        #endif
    }

    private var subtitleTypes: [UTType] {
        SubtitleClient.extensions.compactMap { UTType(filenameExtension: $0) } + [.text]
    }
}
