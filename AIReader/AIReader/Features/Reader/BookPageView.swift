import SwiftUI

#if canImport(UIKit)
import UIKit

/// One page of selectable text that reports the position of a tapped word.
struct BookPageView: UIViewRepresentable {
    let text: NSAttributedString
    let size: CGSize
    let onTap: (_ utf16Offset: Int) -> Void

    func makeUIView(context: Context) -> UITextView {
        let view = UITextView()
        view.isEditable = false
        view.isScrollEnabled = false
        view.backgroundColor = .clear
        view.textContainerInset = .zero
        view.textContainer.lineFragmentPadding = 0
        _ = view.layoutManager  // use TextKit 1, matching the paginator

        let tap = UITapGestureRecognizer(
            target: context.coordinator,
            action: #selector(Coordinator.handleTap)
        )
        tap.delegate = context.coordinator
        view.addGestureRecognizer(tap)
        context.coordinator.textView = view
        return view
    }

    func updateUIView(_ view: UITextView, context: Context) {
        context.coordinator.parent = self
        if view.attributedText != text { view.attributedText = text }
    }

    func sizeThatFits(_ proposal: ProposedViewSize, uiView: UITextView, context: Context) -> CGSize? {
        size
    }

    func makeCoordinator() -> Coordinator { Coordinator(self) }

    final class Coordinator: NSObject, UIGestureRecognizerDelegate {
        var parent: BookPageView
        weak var textView: UITextView?
        private var hadSelection = false

        init(_ parent: BookPageView) { self.parent = parent }

        func gestureRecognizer(
            _ gesture: UIGestureRecognizer,
            shouldRecognizeSimultaneouslyWith other: UIGestureRecognizer
        ) -> Bool { true }

        /// Selecting text must never also fire a lookup.
        func gestureRecognizer(
            _ gesture: UIGestureRecognizer,
            shouldRequireFailureOf other: UIGestureRecognizer
        ) -> Bool {
            guard other.view === textView else { return false }
            if other is UILongPressGestureRecognizer || other is UIPanGestureRecognizer { return true }
            return (other as? UITapGestureRecognizer).map { $0.numberOfTapsRequired > 1 } ?? false
        }

        func gestureRecognizer(
            _ gesture: UIGestureRecognizer,
            shouldReceive touch: UITouch
        ) -> Bool {
            hadSelection = textView?.selectedTextRange.map { !$0.isEmpty } ?? false
            return true
        }

        @objc func handleTap(_ gesture: UITapGestureRecognizer) {
            guard let textView else { return }
            if hadSelection {
                textView.selectedTextRange = nil
                return
            }
            guard let position = textView.closestPosition(to: gesture.location(in: textView))
            else { return }
            parent.onTap(textView.offset(from: textView.beginningOfDocument, to: position))
        }
    }
}

#else
import AppKit

/// One page of text that reports the position of a clicked word. It is not
/// selectable: a selectable text view would take the arrow keys that turn the
/// page.
struct BookPageView: NSViewRepresentable {
    let text: NSAttributedString
    let size: CGSize
    let onTap: (_ utf16Offset: Int) -> Void

    func makeNSView(context: Context) -> NSTextView {
        let view = NSTextView(usingTextLayoutManager: false)  // TextKit 1, matching the paginator
        view.isEditable = false
        view.isSelectable = false
        view.drawsBackground = false
        view.textContainerInset = .zero
        view.textContainer?.lineFragmentPadding = 0
        view.addGestureRecognizer(
            NSClickGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleClick))
        )
        return view
    }

    func updateNSView(_ view: NSTextView, context: Context) {
        context.coordinator.parent = self
        view.textContainer?.containerSize = size
        if view.textStorage?.isEqual(to: text) == false { view.textStorage?.setAttributedString(text) }
    }

    func sizeThatFits(_ proposal: ProposedViewSize, nsView: NSTextView, context: Context) -> CGSize? {
        size
    }

    func makeCoordinator() -> Coordinator { Coordinator(self) }

    final class Coordinator: NSObject {
        var parent: BookPageView

        init(_ parent: BookPageView) { self.parent = parent }

        @objc func handleClick(_ gesture: NSClickGestureRecognizer) {
            guard let view = gesture.view as? NSTextView else { return }
            parent.onTap(view.characterIndexForInsertion(at: gesture.location(in: view)))
        }
    }
}
#endif
