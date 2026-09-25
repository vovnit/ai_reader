import SwiftUI

/// The book as pages. Tapping a word reports it, together with the sentence
/// around it.
struct BookPagesView: View {
    let document: BookDocument
    let style: ReadingStyle
    let startingOffset: Int
    /// A place to turn to, from a search hit or an X-ray passage.
    var jump: ReaderFeature.State.Jump?
    let onPageChanged: (_ offset: Int, _ pageText: String) -> Void
    let onWordTapped: (WordContext.Selection) -> Void

    @State private var pages: [NSRange] = []
    @State private var currentPage = 0
    /// Where the reader is, as a place in the text. It moves only when they
    /// turn the page, so a passing resize cannot move them.
    @State private var anchor: Int?
    @State private var styled: NSAttributedString?
    @FocusState private var isFocused: Bool

    var body: some View {
        GeometryReader { geometry in
            let size = style.pageSize(in: geometry.size)

            Group {
                if pages.isEmpty {
                    ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    pager(size: size)
                }
            }
            .task(id: Layout(size: size, style: style)) { repaginate(size: size) }
            .onChange(of: currentPage) { _, page in
                // A repagination lands on the page holding the anchor; only a
                // real turn goes elsewhere.
                if pages.indices.contains(page), !NSLocationInRange(anchor ?? -1, pages[page]) {
                    anchor = pages[page].location
                }
                report(page)
            }
            .onChange(of: jump) { _, jump in
                guard let jump, let page = pages.firstIndex(where: { NSLocationInRange(jump.offset, $0) }) else { return }
                currentPage = page
            }
        }
        // A keyboard raised in the lookup's chat must not squeeze the page
        // behind it and repaginate the book.
        .ignoresSafeArea(.keyboard)
    }

    #if os(macOS)
    /// A Mac has no page swipe: the arrow keys, space, and the chevrons at the
    /// sides turn the page.
    private func pager(size: CGSize) -> some View {
        HStack(spacing: 0) {
            turn("Previous page", systemImage: "chevron.left", by: -1)
            page(pages[currentPage], size: size)
                .frame(width: size.width, height: size.height)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            turn("Next page", systemImage: "chevron.right", by: 1)
        }
        .focusable()
        .focusEffectDisabled()
        .focused($isFocused)
        .onAppear { isFocused = true }
        .onKeyPress(.leftArrow) { turn(by: -1) }
        .onKeyPress(.rightArrow) { turn(by: 1) }
        .onKeyPress(.space) { turn(by: 1) }
    }

    private func turn(_ title: String, systemImage: String, by step: Int) -> some View {
        Button(title, systemImage: systemImage) { _ = turn(by: step) }
            .labelStyle(.iconOnly)
            .buttonStyle(.plain)
            .foregroundStyle(.secondary)
            .frame(maxHeight: .infinity)
            .padding(.horizontal, 12)
            .contentShape(Rectangle())
            .disabled(!pages.indices.contains(currentPage + step))
    }

    private func turn(by step: Int) -> KeyPress.Result {
        guard pages.indices.contains(currentPage + step) else { return .ignored }
        currentPage += step
        return .handled
    }
    #else
    private func pager(size: CGSize) -> some View {
        TabView(selection: $currentPage) {
            ForEach(pages.indices, id: \.self) { index in
                page(pages[index], size: size)
                    .frame(width: size.width, height: size.height)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .tag(index)
            }
        }
        .tabViewStyle(.page(indexDisplayMode: .never))
    }
    #endif

    private func page(_ range: NSRange, size: CGSize) -> some View {
        BookPageView(
            text: (styled ?? document.text).attributedSubstring(from: range),
            size: size
        ) { offset in
            isFocused = true  // a click on the page must not leave the keys dead
            if let selection = document.words.selection(atUTF16Offset: range.location + offset) {
                onWordTapped(selection)
            }
        }
    }

    /// Keeps the reader on the same passage when the page size changes, and
    /// resumes where the book was left off on first layout.
    private func repaginate(size: CGSize) {
        let anchor = anchor ?? startingOffset
        self.anchor = anchor
        let text = StyledText.apply(style, to: document.text)
        Paginator.fitImages(in: text, to: size)
        styled = text
        let ranges = Paginator.pages(of: text, size: size)
        pages = ranges
        currentPage = ranges.firstIndex { NSLocationInRange(anchor, $0) }
            ?? min(currentPage, max(0, ranges.count - 1))
        report(currentPage)
    }

    private func report(_ index: Int) {
        guard pages.indices.contains(index) else { return }
        let range = pages[index]
        onPageChanged(
            range.location,
            document.text.attributedSubstring(from: range).string
        )
    }
}

/// Anything that changes where the page breaks fall.
private struct Layout: Equatable {
    let width: Int
    let height: Int
    let style: ReadingStyle

    init(size: CGSize, style: ReadingStyle) {
        width = Int(size.width.rounded())
        height = Int(size.height.rounded())
        self.style = style
    }
}
