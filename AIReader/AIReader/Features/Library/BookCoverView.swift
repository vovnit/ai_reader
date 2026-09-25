import SwiftUI

/// A book's cover, or its title on a plain card when it has none.
struct BookCoverView: View {
    let book: Book

    @State private var cover: Image?

    var body: some View {
        ZStack {
            if let cover {
                cover.resizable().scaledToFill()
            } else {
                Rectangle()
                    .fill(.quaternary)
                    .overlay {
                        Text(book.title)
                            .font(.caption)
                            .multilineTextAlignment(.center)
                            .padding(8)
                    }
            }
        }
        .aspectRatio(2 / 3, contentMode: .fit)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .task(id: book.coverPath) {
            cover = book.coverURL.flatMap(Image.init(contentsOf:))
        }
    }
}
