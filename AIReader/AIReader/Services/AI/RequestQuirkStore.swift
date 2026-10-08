import Foundation

/// Remembers which adjustments each endpoint and model needed
/// (`RequestQuirk`), so finding them out — a refused request each — is paid
/// once, not again on every launch, nor every time the Explain extension
/// starts afresh. Kept in the shared suite, so both learn for each other.
actor RequestQuirkStore {
    static let shared = RequestQuirkStore()

    private static let key = "requestQuirks"

    func quirks(for model: String) -> Set<RequestQuirk> {
        Set((stored[model] ?? []).compactMap(RequestQuirk.init(rawValue:)))
    }

    func learn(_ quirk: RequestQuirk, for model: String) {
        var all = stored
        all[model] = Array(Set(all[model] ?? []).union([quirk.rawValue])).sorted()
        AppGroup.defaults.set(all, forKey: Self.key)
    }

    /// Forgets what a model needed, once it no longer fits: a service can
    /// change what it accepts.
    func forget(_ model: String) {
        var all = stored
        all[model] = nil
        AppGroup.defaults.set(all, forKey: Self.key)
    }

    private var stored: [String: [String]] {
        AppGroup.defaults.dictionary(forKey: Self.key) as? [String: [String]] ?? [:]
    }
}
