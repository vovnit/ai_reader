import Foundation
import SQLiteData

/// A set of books read together — a series, a course — so a search from any
/// of them covers them all.
@Table
struct BookGroup: Identifiable, Equatable, Sendable {
    let id: Int
    var name = ""
    var createdAt = Date()
}
