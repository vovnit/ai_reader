import ComposableArchitecture
import SQLiteData
import SwiftUI
import UIKit

/// The extension's entry point: opens the shared database, reads what was
/// shared, and hands over to SwiftUI. Closing the extension is the one thing
/// only this class can do, so it watches for the feature to finish.
final class ActionViewController: UIViewController {
    private var finishObservation: ObserveToken?

    override func viewDidLoad() {
        super.viewDidLoad()
        Self.prepareDependenciesOnce
        Task {
            guard let passage = await SharedItems.passage(from: extensionContext?.inputItems as? [NSExtensionItem] ?? [])
            else { return finish() }
            show(passage)
        }
    }

    /// Dependencies may only be prepared once per process, and the system is
    /// free to reuse the extension's process between invocations.
    private static let prepareDependenciesOnce: Void = {
        prepareDependencies { $0.defaultDatabase = try! appDatabase() }
    }()

    private func show(_ passage: SharedPassage) {
        let store = Store(initialState: ExplainFeature.State(passage: passage)) { ExplainFeature() }
        let host = UIHostingController(rootView: ExplainView(store: store))
        addChild(host)
        host.view.frame = view.bounds
        host.view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        view.addSubview(host.view)
        host.didMove(toParent: self)

        finishObservation = observe { [weak self] in
            if store.isFinished { self?.finish() }
        }
    }

    private func finish() {
        extensionContext?.completeRequest(returningItems: nil)
    }
}
