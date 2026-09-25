import ComposableArchitecture
import SQLiteData
import SwiftUI

@main
struct AIReaderApp: App {
    private let store: StoreOf<LibraryFeature>
    private let subtitles: StoreOf<SubtitlesFeature>

    init() {
        AppGroup.adoptLegacyFiles()
        prepareDependencies { $0.defaultDatabase = try! appDatabase() }
        store = Store(initialState: LibraryFeature.State()) { LibraryFeature() }
        subtitles = Store(initialState: SubtitlesFeature.State()) { SubtitlesFeature() }
    }

    var body: some Scene {
        WindowGroup {
            LibraryView(store: store)
        }
        #if os(macOS)
        // Wide enough for a comfortable page beside the lookup panel.
        .defaultSize(width: 1180, height: 820)
        #endif
        #if os(macOS)
        // A small panel that stays over VLC's window, opened from the library.
        Window("Subtitles", id: SubtitlesView.windowID) {
            SubtitlesView(store: subtitles)
        }
        .windowLevel(.floating)
        .defaultSize(width: 520, height: 180)
        #endif
    }
}
