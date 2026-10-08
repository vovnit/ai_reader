import AIReaderCore
import Foundation

/// The sync records kept as many small files rather than one: `00.json` to
/// `ff.json` in the folder `aireader-sync`, each record's file following
/// from its key. A device remembers each file's version on the server and a
/// fingerprint of its own records there, so a sync reads only the files
/// another device changed and sends only those its own changes fall in. The
/// one file older versions kept everything in is still read whenever it
/// changes, and never written.
///
/// The rules are the Kindle app's C++, shared through `Core/`; this is their
/// Swift face, as `SyncDocument` is the document's.
enum SyncParts {
    static let folder = String(cString: AIReaderCore.SyncParts.folder)
    static let oldFile = String(cString: AIReaderCore.SyncParts.oldFile)

    /// One file's records.
    struct Part: Equatable, Sendable {
        var name: String
        var records = SyncDocument()
    }

    /// A file as the server lists it, or as this device last knew it.
    struct File: Equatable, Sendable {
        var name: String
        /// The server's version of it; empty when the server did not say.
        var version = ""
        /// This device's records that belong in it, fingerprinted; empty in
        /// a listing, and when there are none.
        var digest = ""
    }

    /// The files a sync must read: changed or gone on the server since this
    /// device last knew them, holding records that changed here, or holding
    /// records of the old file.
    static func due(listed: [File], known: [File], local: SyncDocument, old: SyncDocument) -> [String] {
        AIReaderCore.SyncParts.due(listed.core, known.core, local.core, old.core).map { String($0) }
    }

    /// Each due file merged: the server's records (`remote`, one part per due
    /// file), this device's, and the old file's. In the order of `remote`.
    static func merge(_ remote: [Part], local: SyncDocument, old: SyncDocument) -> [Part] {
        AIReaderCore.SyncParts.merge(remote.core, local.core, old.core).map(Part.init)
    }

    /// The parts' records together.
    static func join(_ parts: [Part]) -> SyncDocument {
        SyncDocument(AIReaderCore.SyncParts.join(parts.core))
    }

    /// Each file's fingerprint of `local`, without versions.
    static func fingerprints(_ local: SyncDocument) -> [File] {
        AIReaderCore.SyncParts.fingerprints(local.core).map(File.init)
    }

    /// What this device knows of the files once `settled` have been dealt
    /// with: their entries replace those in `known`, and one with neither a
    /// version nor a digest is dropped.
    static func remember(_ known: [File], settled: [File]) -> [File] {
        AIReaderCore.SyncParts.remember(known.core, settled.core).map(File.init)
    }
}
