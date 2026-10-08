import Foundation
import SQLiteData

/// What this device last knew of a file in the sync folder's
/// `aireader-sync` (`SyncParts.File`), kept with the folder's address, since
/// another folder's versions say nothing.
@Table("syncFiles")
struct SyncFile: Equatable, Sendable {
    var folder = ""
    var name = ""
    var version = ""
    var digest = ""
}
