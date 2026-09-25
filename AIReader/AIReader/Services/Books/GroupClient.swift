import ComposableArchitecture
import Foundation
import SQLiteData

/// The groups books are read in: putting a book in one, naming a new one,
/// dissolving one.
@DependencyClient
struct GroupClient: Sendable {
    /// Puts the book in a group; nil takes it out of any.
    var assign: @Sendable (_ bookID: Book.ID, _ groupID: BookGroup.ID?) async throws -> Void
    /// The group of that name, made if there is none yet.
    var named: @Sendable (_ name: String) async throws -> BookGroup.ID
    /// Dissolves the group; its books stay on the shelf, ungrouped.
    var dissolve: @Sendable (_ groupID: BookGroup.ID) async throws -> Void
}

extension GroupClient: DependencyKey {
    static var liveValue: Self {
        Self(
            assign: { bookID, groupID in
                @Dependency(\.defaultDatabase) var database
                @Dependency(\.date.now) var now
                try await database.write { db in
                    try Book
                        .update {
                            $0.groupID = #bind(groupID)
                            $0.updatedAt = #bind(now)
                        }
                        .where { $0.id.eq(bookID) }
                        .execute(db)
                }
            },
            named: { name in
                @Dependency(\.defaultDatabase) var database
                @Dependency(\.date.now) var now
                return try await database.write { db -> BookGroup.ID in
                    if let existing = try BookGroup.where { $0.name.eq(name) }.fetchOne(db) {
                        return existing.id
                    }
                    let inserted = try BookGroup
                        .insert { BookGroup.Draft(name: name, createdAt: now) }
                        .returning { $0.id }
                        .fetchOne(db)
                    return inserted ?? 0
                }
            },
            dissolve: { groupID in
                @Dependency(\.defaultDatabase) var database
                @Dependency(\.date.now) var now
                try await database.write { db in
                    try Book
                        .update {
                            $0.groupID = #bind(nil)
                            $0.updatedAt = #bind(now)
                        }
                        .where { $0.groupID.eq(groupID) }
                        .execute(db)
                    try BookGroup.delete().where { $0.id.eq(groupID) }.execute(db)
                }
            }
        )
    }

    static let testValue = Self()
}

extension DependencyValues {
    var groupClient: GroupClient {
        get { self[GroupClient.self] }
        set { self[GroupClient.self] = newValue }
    }
}
