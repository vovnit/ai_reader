#if os(iOS)
import ComposableArchitecture
import PDFKit
import SwiftUI
import UIKit

/// A PDF book's pages as the PDF draws them, one at a time, turned with a
/// swipe or the buttons at the foot; a tapped word is explained in a sheet
/// and marked on the page while it is open.
struct PDFPagesView: View {
    @Bindable var store: StoreOf<PDFPagesFeature>
    @State private var document: PDFDocument?

    var body: some View {
        content
            .navigationTitle(store.pageCount > 0 ? "Page \(store.page + 1) of \(store.pageCount)" : "Pages")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItemGroup(placement: .bottomBar) {
                    Button("Previous page", systemImage: "chevron.left") { store.send(.previousTapped) }
                        .disabled(store.page == 0)
                    Spacer()
                    Button("Next page", systemImage: "chevron.right") { store.send(.nextTapped) }
                        .disabled(store.page + 1 >= store.pageCount)
                }
            }
            .sheet(item: $store.scope(state: \.lookup, action: \.lookup)) { lookup in
                LookupView(store: lookup)
            }
            .task {
                if document == nil { document = PDFDocument(url: store.pdf) }
                await store.send(.task).finish()
            }
    }

    @ViewBuilder
    private var content: some View {
        if let message = store.errorMessage {
            ContentUnavailableView("Couldn’t open the pages", systemImage: "doc.richtext", description: Text(message))
        } else if let document, store.pageCount > 0 {
            PDFPageView(
                document: document,
                page: store.page,
                marked: store.marked,
                onPageShown: { store.send(.pageShown($0)) },
                onTap: { store.send(.characterTapped($0)) }
            )
        } else {
            ProgressView()
        }
    }
}

/// PDFKit's own view of the document, a page at a time: it draws the page,
/// turns it with a swipe, finds the character under a tap and marks a word.
private struct PDFPageView: UIViewRepresentable {
    let document: PDFDocument
    let page: Int
    let marked: NSRange?
    let onPageShown: (Int) -> Void
    let onTap: (Int) -> Void

    func makeCoordinator() -> Coordinator { Coordinator(self) }

    func makeUIView(context: Context) -> PDFView {
        let view = PDFView()
        view.displayMode = .singlePage
        view.displayDirection = .horizontal
        view.autoScales = true
        view.usePageViewController(true, withViewOptions: nil)
        view.document = document
        let tap = UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.tapped(_:)))
        tap.delegate = context.coordinator
        view.addGestureRecognizer(tap)
        NotificationCenter.default.addObserver(
            context.coordinator,
            selector: #selector(Coordinator.pageChanged(_:)),
            name: .PDFViewPageChanged,
            object: view
        )
        return view
    }

    func updateUIView(_ view: PDFView, context: Context) {
        context.coordinator.parent = self
        guard let shown = document.page(at: page) else { return }
        if view.currentPage != shown { view.go(to: shown) }
        if view.currentPage == shown { context.coordinator.settled = true }
        let selection = marked.flatMap { shown.selection(for: $0) }
        selection?.color = UIColor.tintColor.withAlphaComponent(0.35)
        view.highlightedSelections = selection.map { [$0] }
    }

    final class Coordinator: NSObject, UIGestureRecognizerDelegate {
        var parent: PDFPageView
        /// Whether the view has reached the page it opened at; the pages it
        /// passes on the way there are not the reader's turning.
        var settled = false

        init(_ parent: PDFPageView) {
            self.parent = parent
        }

        @objc func pageChanged(_ notification: Notification) {
            guard let view = notification.object as? PDFView,
                  let page = view.currentPage,
                  let document = view.document
            else { return }
            let index = document.index(for: page)
            guard settled else {
                settled = index == parent.page
                return
            }
            parent.onPageShown(index)
        }

        @objc func tapped(_ gesture: UITapGestureRecognizer) {
            guard let view = gesture.view as? PDFView else { return }
            let point = gesture.location(in: view)
            guard let page = view.page(for: point, nearest: false),
                  let document = view.document,
                  document.index(for: page) == parent.page
            else { return }
            let index = page.characterIndex(at: view.convert(point, to: page))
            // No character there: between words, a picture, the margin.
            guard index >= 0, index != NSNotFound else { return }
            parent.onTap(index)
        }

        // PDFKit's own gestures — a double tap to zoom, a drag to turn — still work.
        func gestureRecognizer(
            _ gestureRecognizer: UIGestureRecognizer,
            shouldRecognizeSimultaneouslyWith other: UIGestureRecognizer
        ) -> Bool {
            true
        }
    }
}
#endif
