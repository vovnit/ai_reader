import ComposableArchitecture
import Foundation

/// Finds and reads subtitle files, on this machine or on the WebDAV server
/// the sync settings name.
@DependencyClient
struct SubtitleClient: Sendable {
    /// The subtitle file beside a video, named after it: `film.srt`,
    /// `film.en.srt`. The bare name wins when there are several.
    var find: @Sendable (_ video: URL) async throws -> URL?
    var load: @Sendable (_ url: URL) async throws -> SubtitleTrack
    /// What a WebDAV folder holds, for choosing a file by hand.
    var browse: @Sendable (_ folder: URL) async throws -> [WebDAVEntry]
    /// The WebDAV folder a video Kodi is playing sits in, on the sync server;
    /// the sync folder itself when nothing is playing.
    var folder: @Sendable (_ video: URL?) -> URL?
}

extension SubtitleClient: DependencyKey {
    static let extensions = ["srt", "vtt"]

    static let liveValue: Self = {
        @Dependency(\.syncSettingsClient) var syncSettings
        return Self(
            find: { video in
                if video.isFileURL {
                    let files = (try? FileManager.default.contentsOfDirectory(
                        at: video.deletingLastPathComponent(), includingPropertiesForKeys: nil
                    )) ?? []
                    return match(video, among: files)
                }
                guard let folder = remoteFolder(of: video, syncSettings.load()) else { return nil }
                let entries = try await WebDAV.list(folder, settings: syncSettings.load())
                return match(video, among: entries.filter { !$0.isFolder }.map(\.url))
            },
            load: { url in
                let text: String
                if url.isFileURL {
                    // A file the system picker handed over may only be read
                    // while its scope is open.
                    let scoped = url.startAccessingSecurityScopedResource()
                    defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                    text = try TextFile.text(at: url)
                } else {
                    guard let data = try await WebDAV.download(url, settings: syncSettings.load()) else {
                        throw WebDAV.DAVError.http(status: 404)
                    }
                    text = TextFile.decode(data)
                }
                return SubtitleTrack(cues: SubtitleFile.cues(in: text))
            },
            browse: { folder in
                try await WebDAV.list(folder, settings: syncSettings.load())
            },
            folder: { video in
                let settings = syncSettings.load()
                return video.flatMap { remoteFolder(of: $0, settings) } ?? settings.folderURL
            }
        )
    }()

    static let testValue = Self()

    private static func match(_ video: URL, among files: [URL]) -> URL? {
        let stem = video.deletingPathExtension().lastPathComponent.lowercased()
        return files
            .filter { extensions.contains($0.pathExtension.lowercased()) }
            .filter { $0.deletingPathExtension().lastPathComponent.lowercased().hasPrefix(stem) }
            .min { $0.lastPathComponent.count < $1.lastPathComponent.count }
    }

    /// Kodi names the file by its own address (`dav://box/Films/x.mkv`); the
    /// same path on the sync server is where the app can read beside it.
    private static func remoteFolder(of video: URL, _ settings: SyncSettings) -> URL? {
        guard let server = settings.serverURL else { return nil }
        let folder = video.deletingLastPathComponent().path
        return server.appending(path: folder.trimmingCharacters(in: CharacterSet(charactersIn: "/")), directoryHint: .isDirectory)
    }
}

extension DependencyValues {
    var subtitleClient: SubtitleClient {
        get { self[SubtitleClient.self] }
        set { self[SubtitleClient.self] = newValue }
    }
}
