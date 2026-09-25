import ComposableArchitecture
import Foundation

/// Follows a video playing in VLC or Kodi and shows its current subtitle
/// line as words to tap. A tap pauses the player and opens the same lookup
/// panel a book gives, with the sentence gathered from the cues around the
/// line.
@Reducer
struct SubtitlesFeature {
    @ObservableState
    struct State: Equatable {
        var settings = PlayerSettings()
        /// Shown instead of the line, so the settings can be changed after
        /// the player was reached.
        var isEditingConnection = false
        var isFindingKodi = false
        var errorMessage: String?
        /// Polls that failed in a row. One is a busy player, not a lost one.
        var failures = 0
        var video: URL?
        var subtitles: URL?
        var track: SubtitleTrack?
        var status: PlayerStatus?
        var cueIndex: Int?
        var isImporterPresented = false
        /// Set while the player was paused for a lookup and should resume
        /// after it.
        var pausedForLookup = false
        @Presents var lookup: LookupFeature.State?
        @Presents var picker: WebDAVPickerFeature.State?

        var cue: SubtitleCue? {
            guard let track, let cueIndex else { return nil }
            return track.cues[cueIndex]
        }

        var chunks: [WordContext.Chunk] {
            cue.map { WordContext(text: $0.text).chunks } ?? []
        }
    }

    enum Action: BindableAction {
        case task
        case binding(BindingAction<State>)
        case statusReceived(PlayerStatus)
        case statusFailed(String)
        case videoChanged(URL?)
        case subtitlesFound(URL?)
        case subtitlesLoaded(Result<SubtitleTrack, any Error>, url: URL)
        case editConnectionTapped
        case findKodiTapped
        case kodiFound(PlayerAddress?)
        case chooseSubtitlesTapped(SubtitleSource)
        case subtitlesPicked(Result<URL, any Error>)
        case wordTapped(utf16Offset: Int)
        case lookup(PresentationAction<LookupFeature.Action>)
        case picker(PresentationAction<WebDAVPickerFeature.Action>)
    }

    /// Where a subtitle file chosen by hand comes from.
    enum SubtitleSource: Equatable {
        /// The device's own files, through the system picker.
        case device
        /// The WebDAV server the sync settings name.
        case server
    }

    private enum CancelID { case polling }

    @Dependency(\.continuousClock) var clock
    @Dependency(\.playerClient) var player
    @Dependency(\.playerSettingsClient) var playerSettings
    @Dependency(\.subtitleClient) var subtitles
    @Dependency(\.uuid) var uuid

    var body: some ReducerOf<Self> {
        CombineReducers {
            BindingReducer()
            Reduce { state, action in
                switch action {
                case .task:
                    state.settings = playerSettings.load()
                    if state.settings.kind == .kodi, state.settings.host.isEmpty {
                        return .send(.findKodiTapped)
                    }
                    return poll(state.settings)

                case let .statusReceived(status):
                    let fileChanged = status.item != state.status?.item
                    state.status = status
                    state.errorMessage = nil
                    state.failures = 0
                    state.cueIndex = state.track?.index(at: status.time)
                    guard fileChanged else { return .none }
                    let settings = state.settings
                    return .run { send in
                        await send(.videoChanged(try? await player.nowPlaying(settings)))
                    }

                case let .statusFailed(message):
                    state.failures += 1
                    guard state.failures >= 3 else { return .none }
                    state.errorMessage = message
                    state.status = nil
                    state.cueIndex = nil
                    return .none

                case let .videoChanged(url):
                    state.video = url
                    state.track = nil
                    state.subtitles = nil
                    state.cueIndex = nil
                    guard let url else { return .none }
                    return .run { send in
                        await send(.subtitlesFound(try? await subtitles.find(url)))
                    }

                case let .subtitlesFound(url):
                    return url.map(load) ?? .none

                case let .subtitlesLoaded(.success(track), url):
                    state.track = track
                    state.subtitles = url
                    state.cueIndex = state.status.flatMap { track.index(at: $0.time) }
                    return .none

                case let .subtitlesLoaded(.failure(error), _):
                    state.errorMessage = error.localizedDescription
                    return .none

                case .editConnectionTapped:
                    state.isEditingConnection.toggle()
                    return .none

                case .findKodiTapped:
                    state.isFindingKodi = true
                    return .run { send in
                        await send(.kodiFound(await player.findKodi()))
                    }

                case let .kodiFound(address):
                    state.isFindingKodi = false
                    guard let address else {
                        state.errorMessage = "No Kodi announced itself. Check that Zeroconf is on in its Services settings, or enter the address."
                        return .none
                    }
                    state.settings.host = address.host
                    state.settings.port = String(address.port)
                    return .none

                case .chooseSubtitlesTapped(.device):
                    state.isImporterPresented = true
                    return .none

                case .chooseSubtitlesTapped(.server):
                    guard let folder = subtitles.folder(state.video) else {
                        state.errorMessage = "Set the WebDAV server in Settings first."
                        return .none
                    }
                    state.picker = WebDAVPickerFeature.State(folder: folder)
                    return .none

                case let .subtitlesPicked(.success(url)):
                    return load(url)

                case let .subtitlesPicked(.failure(error)):
                    state.errorMessage = error.localizedDescription
                    return .none

                case let .picker(.presented(.delegate(.picked(url)))):
                    state.picker = nil
                    return load(url)

                case .picker(.presented(.delegate(.dismiss))):
                    state.picker = nil
                    return .none

                case let .wordTapped(offset):
                    guard let track = state.track, let cueIndex = state.cueIndex else { return .none }
                    let passage = track.passage(around: cueIndex)
                    guard let selection = WordContext(text: passage.text)
                        .selection(atUTF16Offset: passage.cueOffset + offset)
                    else { return .none }
                    state.lookup = LookupFeature.State(
                        id: uuid(),
                        context: LookupContext(word: selection.word, sentence: selection.sentence, language: track.language)
                    )
                    guard state.status?.state == .playing else { return .none }
                    state.pausedForLookup = true
                    let settings = state.settings
                    return .run { _ in try? await player.pause(settings) }

                case .lookup(.dismiss):
                    guard state.pausedForLookup else { return .none }
                    state.pausedForLookup = false
                    let settings = state.settings
                    return .run { _ in try? await player.resume(settings) }

                case .binding, .lookup, .picker:
                    return .none
                }
            }
        }
        .onChange(of: \.settings) { _, settings in
            Reduce { state, _ in
                state.errorMessage = nil
                playerSettings.save(settings)
                return poll(settings)
            }
        }
        .ifLet(\.$lookup, action: \.lookup) {
            LookupFeature()
        }
        .ifLet(\.$picker, action: \.picker) {
            WebDAVPickerFeature()
        }
    }

    /// Asks the player where it is a few times a second; less often while it
    /// cannot be reached, so a closed player costs nothing to wait for.
    private func poll(_ settings: PlayerSettings) -> Effect<Action> {
        .run { send in
            while !Task.isCancelled {
                do {
                    await send(.statusReceived(try await player.status(settings)))
                    try await clock.sleep(for: .milliseconds(250))
                } catch is CancellationError {
                    return
                } catch {
                    await send(.statusFailed(error.localizedDescription))
                    try? await clock.sleep(for: .seconds(1))
                }
            }
        }
        .cancellable(id: CancelID.polling, cancelInFlight: true)
    }

    private func load(_ url: URL) -> Effect<Action> {
        .run { send in
            await send(.subtitlesLoaded(Result { try await subtitles.load(url) }, url: url))
        }
    }
}
