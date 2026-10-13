#if os(iOS)
import SwiftUI
import UIKit

/// The photo with the text read off it laid over its print, each word where
/// it was read, to tap one.
struct PhotoWordsView: View {
    let photo: Data
    let passage: PhotoPassage
    let onTap: (_ utf16Offset: Int) -> Void
    @State private var image: UIImage?

    var body: some View {
        let size = passage.photoSize
        Color.clear
            .aspectRatio(size, contentMode: .fit)
            .background {
                if let image { Image(uiImage: image).resizable() }
            }
            .overlay {
                GeometryReader { proxy in
                    let scale = proxy.size.width / size.width
                    ForEach(passage.words.indices, id: \.self) { index in
                        word(passage.words[index], scale: scale)
                    }
                }
            }
            .task(id: photo) {
                // Decoded once and no larger than a screen shows it, rather
                // than at the camera's full size on every redraw.
                guard let full = UIImage(data: photo) else { return }
                let shrink = min(1, 2048 / max(full.size.width, full.size.height))
                image = await full.byPreparingThumbnail(
                    ofSize: CGSize(width: full.size.width * shrink, height: full.size.height * shrink)
                )
            }
    }

    private func word(_ word: PhotoWord, scale: CGFloat) -> some View {
        Button { onTap(word.offset) } label: {
            Text(word.text)
                .font(.system(size: word.size.height * scale * 0.8))
                .lineLimit(1)
                .minimumScaleFactor(0.1)
                .frame(width: word.size.width * scale, height: word.size.height * scale)
                .background(.background)
        }
        .buttonStyle(.plain)
        .rotationEffect(.radians(word.angle))
        .position(x: word.center.x * scale, y: word.center.y * scale)
    }
}
#endif
