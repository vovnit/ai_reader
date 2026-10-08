import ComposableArchitecture
import Foundation

/// The questions asked before a book's file leaves the sync folder: the
/// other devices can no longer fetch it, so it is not done on one tap.
extension AlertState where Action == LibraryFeature.Alert {
    static func deleteEverywhere(_ book: Book) -> Self {
        Self {
            TextState("Delete “\(book.title)” here and from the sync folder?")
        } actions: {
            ButtonState(role: .destructive, action: .deleteEverywhere(book)) { TextState("Delete") }
            ButtonState(role: .cancel) { TextState("Cancel") }
        } message: {
            TextState("Devices that already have it keep their copy. Words looked up in it are kept.")
        }
    }

    static func deleteRemote(_ book: RemoteBook) -> Self {
        Self {
            TextState("Delete “\(book.title)” from the sync folder?")
        } actions: {
            ButtonState(role: .destructive, action: .deleteRemote(book)) { TextState("Delete") }
            ButtonState(role: .cancel) { TextState("Cancel") }
        } message: {
            TextState("Devices that already have it keep their copy.")
        }
    }
}
