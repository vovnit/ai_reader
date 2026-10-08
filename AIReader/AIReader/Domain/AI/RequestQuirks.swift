import Foundation

/// Parameters that some OpenAI-compatible services reject while others require
/// them. A service names the offending parameter in its error, so a request can
/// be adjusted and tried again; what a model needed is remembered by
/// `RequestQuirkStore`.
enum RequestQuirk: String, CaseIterable, Sendable {
    /// Newer OpenAI reasoning models take `max_completion_tokens` in place of
    /// `max_tokens`.
    case completionTokens
    /// Those models also only accept their default temperature.
    case defaultTemperature
    /// Some of them refuse function tools unless reasoning is switched off.
    case noReasoning

    /// Reads the quirk a failed request's error body is describing, if any.
    static func named(inErrorBody body: String) -> RequestQuirk? {
        guard let data = body.data(using: .utf8),
              let failure = try? JSONDecoder().decode(Failure.self, from: data)
        else { return nil }

        let parameter = failure.error.param ?? ""
        let message = failure.error.message ?? ""
        if parameter == "max_tokens" || message.contains("max_completion_tokens") {
            return .completionTokens
        }
        if parameter == "temperature" {
            return .defaultTemperature
        }
        if parameter == "reasoning_effort" {
            return .noReasoning
        }
        return nil
    }

    private struct Failure: Decodable {
        struct Error: Decodable {
            let message: String?
            let param: String?
        }
        let error: Error
    }
}
