import Foundation

/// A file or folder a WebDAV server lists.
struct WebDAVEntry: Equatable, Sendable, Identifiable {
    let url: URL
    let isFolder: Bool

    var id: URL { url }
    var name: String { url.lastPathComponent }
}

/// What the app needs of a WebDAV server: fetch one file, store one file,
/// and list a folder. Any server that speaks HTTP GET, PUT and PROPFIND with
/// a password will do.
enum WebDAV {
    enum DAVError: LocalizedError {
        case unauthorized
        case http(status: Int)

        var errorDescription: String? {
            switch self {
            case .unauthorized: "The server refused the user name or password."
            case let .http(status): "The server answered \(status)."
            }
        }
    }

    /// The file's bytes, or nil when there is no such file yet.
    static func download(_ url: URL, settings: SyncSettings) async throws -> Data? {
        let (data, status) = try await send("GET", url, body: nil, settings: settings)
        if status == 404 { return nil }
        try check(status)
        return data
    }

    /// Stores the file, making its folder first if the server says there is
    /// none.
    static func upload(
        _ data: Data,
        to url: URL,
        type: String = "application/json",
        settings: SyncSettings
    ) async throws {
        let (_, status) = try await send("PUT", url, body: data, type: type, settings: settings)
        if status == 409 || status == 404 {
            try await makeFolder(url.deletingLastPathComponent(), settings: settings)
            let (_, again) = try await send("PUT", url, body: data, type: type, settings: settings)
            try check(again)
            return
        }
        try check(status)
    }

    /// The folder's own entries, folders first.
    static func list(_ folder: URL, settings: SyncSettings) async throws -> [WebDAVEntry] {
        let (data, status) = try await send("PROPFIND", folder, body: nil, depth: "1", settings: settings)
        try check(status)
        var entries: [WebDAVEntry] = []
        var href = ""
        var isFolder = false
        func close() {
            if let url = URL(string: href, relativeTo: folder)?.absoluteURL,
               url.standardized.path != folder.standardized.path {
                entries.append(WebDAVEntry(url: url, isFolder: isFolder))
            }
            href = ""
            isFolder = false
        }
        XMLScanner.scan(data) { name, _ in
            if name == "response", !href.isEmpty { close() }
            if name == "collection" { isFolder = true }
        } onText: { name, text in
            if name == "href" { href = text.trimmingCharacters(in: .whitespacesAndNewlines) }
        }
        if !href.isEmpty { close() }
        return entries.sorted { ($0.isFolder ? 0 : 1, $0.name) < ($1.isFolder ? 0 : 1, $1.name) }
    }

    /// Makes the folder, and the folders above it that are missing too: a
    /// server makes only one level at a time, answering 409 when the one
    /// above is not there.
    private static func makeFolder(_ url: URL, settings: SyncSettings, depth: Int = 0) async throws {
        let (_, status) = try await send("MKCOL", url, body: nil, settings: settings)
        guard status == 409, depth < 8 else { return }
        try await makeFolder(url.deletingLastPathComponent(), settings: settings, depth: depth + 1)
        _ = try await send("MKCOL", url, body: nil, settings: settings)
    }

    private static func send(
        _ method: String,
        _ url: URL,
        body: Data?,
        type: String = "application/json",
        depth: String? = nil,
        settings: SyncSettings
    ) async throws -> (Data, Int) {
        var request = URLRequest(url: url)
        request.httpMethod = method
        request.httpBody = body
        request.timeoutInterval = 30
        if let depth { request.setValue(depth, forHTTPHeaderField: "Depth") }
        if !settings.username.isEmpty || !settings.password.isEmpty {
            let credentials = Data("\(settings.username):\(settings.password)".utf8).base64EncodedString()
            request.setValue("Basic \(credentials)", forHTTPHeaderField: "Authorization")
        }
        if body != nil {
            request.setValue(type, forHTTPHeaderField: "Content-Type")
        }
        let (data, response) = try await URLSession.shared.data(for: request)
        return (data, (response as? HTTPURLResponse)?.statusCode ?? 0)
    }

    private static func check(_ status: Int) throws {
        if status == 401 || status == 403 { throw DAVError.unauthorized }
        guard (200...299).contains(status) else { throw DAVError.http(status: status) }
    }
}
