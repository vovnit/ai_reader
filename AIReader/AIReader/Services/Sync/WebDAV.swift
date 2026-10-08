import Foundation

/// A file or folder a WebDAV server lists.
struct WebDAVEntry: Equatable, Sendable, Identifiable {
    let url: URL
    let isFolder: Bool
    /// The version the server gives it (its ETag); empty when it gives none.
    var version = ""

    var id: URL { url }
    var name: String { url.lastPathComponent }
}

/// What the app needs of a WebDAV server: fetch, store and delete one file,
/// and list a folder. Any server that speaks HTTP GET, PUT, DELETE and
/// PROPFIND with a password will do.
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
        let (data, status, _) = try await send("GET", url, body: nil, settings: settings)
        if status == 404 { return nil }
        try check(status)
        return data
    }

    /// Stores the file, making its folder first if the server says there is
    /// none. Returns the version the server gives it, or "" when it gives
    /// none.
    @discardableResult
    static func upload(
        _ data: Data,
        to url: URL,
        type: String = "application/json",
        settings: SyncSettings
    ) async throws -> String {
        let (_, status, etag) = try await send("PUT", url, body: data, type: type, settings: settings)
        if status == 409 || status == 404 {
            try await makeFolder(url.deletingLastPathComponent(), settings: settings)
            let (_, again, etag) = try await send("PUT", url, body: data, type: type, settings: settings)
            try check(again)
            return etag
        }
        try check(status)
        return etag
    }

    /// Deletes the file; one already gone counts as deleted.
    static func delete(_ url: URL, settings: SyncSettings) async throws {
        let (_, status, _) = try await send("DELETE", url, body: nil, settings: settings)
        if status == 404 { return }
        try check(status)
    }

    /// The folder's own entries, folders first.
    static func list(_ folder: URL, settings: SyncSettings) async throws -> [WebDAVEntry] {
        let (data, status, _) = try await send("PROPFIND", folder, body: nil, depth: "1", settings: settings)
        try check(status)
        var entries: [WebDAVEntry] = []
        var href = ""
        var version = ""
        var isFolder = false
        func close() {
            if let url = URL(string: href, relativeTo: folder)?.absoluteURL,
               url.standardized.path != folder.standardized.path {
                entries.append(WebDAVEntry(url: url, isFolder: isFolder, version: version))
            }
            href = ""
            version = ""
            isFolder = false
        }
        XMLScanner.scan(data) { name, _ in
            if name == "response", !href.isEmpty { close() }
            if name == "collection" { isFolder = true }
        } onText: { name, text in
            if name == "href" { href = text.trimmingCharacters(in: .whitespacesAndNewlines) }
            if name == "getetag" { version = text }
        }
        if !href.isEmpty { close() }
        return entries.sorted { ($0.isFolder ? 0 : 1, $0.name) < ($1.isFolder ? 0 : 1, $1.name) }
    }

    /// The version the server gives the file ("" when it gives none), or nil
    /// when there is no such file.
    static func version(of url: URL, settings: SyncSettings) async throws -> String? {
        let (data, status, _) = try await send("PROPFIND", url, body: nil, depth: "0", settings: settings)
        if status == 404 { return nil }
        try check(status)
        var version = ""
        XMLScanner.scan(data) { _, _ in } onText: { name, text in
            if name == "getetag" { version = text }
        }
        return version
    }

    /// Makes the folder, and the folders above it that are missing too: a
    /// server makes only one level at a time, answering 409 when the one
    /// above is not there.
    private static func makeFolder(_ url: URL, settings: SyncSettings, depth: Int = 0) async throws {
        let (_, status, _) = try await send("MKCOL", url, body: nil, settings: settings)
        guard status == 409, depth < 8 else { return }
        try await makeFolder(url.deletingLastPathComponent(), settings: settings, depth: depth + 1)
        _ = try await send("MKCOL", url, body: nil, settings: settings)
    }

    /// The body, the status, and the version the server gave in an `ETag`.
    private static func send(
        _ method: String,
        _ url: URL,
        body: Data?,
        type: String = "application/json",
        depth: String? = nil,
        settings: SyncSettings
    ) async throws -> (Data, Int, String) {
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
        let http = response as? HTTPURLResponse
        return (data, http?.statusCode ?? 0, http?.value(forHTTPHeaderField: "ETag") ?? "")
    }

    private static func check(_ status: Int) throws {
        if status == 401 || status == 403 { throw DAVError.unauthorized }
        guard (200...299).contains(status) else { throw DAVError.http(status: status) }
    }
}
