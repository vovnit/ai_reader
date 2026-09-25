import AVFoundation
import ComposableArchitecture

/// Reads text aloud in the language of the book, so a learner can hear a word
/// as well as read it.
@DependencyClient
struct SpeechClient: Sendable {
    var speak: @Sendable (_ text: String, _ language: String?) -> Void
    var stop: @Sendable () -> Void
}

extension SpeechClient: DependencyKey {
    static let liveValue = Self(
        speak: { text, language in
            Speaker.shared.speak(text, language: language)
        },
        stop: { Speaker.shared.stop() }
    )

    static let testValue = Self()
}

extension DependencyValues {
    var speechClient: SpeechClient {
        get { self[SpeechClient.self] }
        set { self[SpeechClient.self] = newValue }
    }
}

private final class Speaker: NSObject, AVSpeechSynthesizerDelegate, @unchecked Sendable {
    static let shared = Speaker()

    private let synthesizer = AVSpeechSynthesizer()

    override init() {
        super.init()
        synthesizer.delegate = self
    }

    func speak(_ text: String, language: String?) {
        stop()
        activateAudio()

        let utterance = AVSpeechUtterance(string: text)
        utterance.voice = Self.voice(for: language)
        // A little under natural pace: this is for hearing a word clearly, not
        // for listening to a book.
        utterance.rate = AVSpeechUtteranceDefaultSpeechRate * 0.9
        synthesizer.speak(utterance)
    }

    func stop() {
        synthesizer.stopSpeaking(at: .immediate)
    }

    /// Picks a voice for a language tag that may be as vague as "fr".
    static func voice(for language: String?) -> AVSpeechSynthesisVoice? {
        guard let language, !language.isEmpty else { return nil }
        let tag = language.replacingOccurrences(of: "_", with: "-")
        guard !tag.contains("-") else { return AVSpeechSynthesisVoice(language: tag) }

        let primary = tag.lowercased()
        let candidates = AVSpeechSynthesisVoice.speechVoices()
            .filter { $0.language.lowercased().hasPrefix("\(primary)-") }
        // Prefer the language's home region — fr-FR over fr-CA — since that is
        // what a learner is most likely studying.
        let home = "\(primary)-\(primary)"
        return candidates.first { $0.language.lowercased() == home }
            ?? candidates.first
            ?? AVSpeechSynthesisVoice(language: primary)
    }

    // MARK: - Audio session

    /// Speech belongs to the playback category, otherwise the ring/silent
    /// switch mutes it and nothing appears to happen.
    private func activateAudio() {
        #if os(iOS) || os(visionOS)
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playback, mode: .spokenAudio, options: [.duckOthers])
        try? session.setActive(true)
        #endif
    }

    private func deactivateAudio() {
        #if os(iOS) || os(visionOS)
        // Let whatever was playing before come back up.
        try? AVAudioSession.sharedInstance()
            .setActive(false, options: .notifyOthersOnDeactivation)
        #endif
    }

    func speechSynthesizer(
        _ synthesizer: AVSpeechSynthesizer,
        didFinish utterance: AVSpeechUtterance
    ) {
        deactivateAudio()
    }

    func speechSynthesizer(
        _ synthesizer: AVSpeechSynthesizer,
        didCancel utterance: AVSpeechUtterance
    ) {
        deactivateAudio()
    }
}
