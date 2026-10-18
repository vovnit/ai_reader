import Foundation

/// Chat completions and the model list, against any OpenAI-compatible service,
/// or Anthropic's in its own Messages API. Request bodies are assembled as
/// plain JSON so tool schemas stay readable.
enum ChatAPI {
    enum APIError: LocalizedError {
        case invalidEndpoint(String)
        case rateLimited
        case http(status: Int, body: String)
        case noChoices

        var errorDescription: String? {
            switch self {
            case let .invalidEndpoint(endpoint): "“\(endpoint)” is not a valid endpoint URL."
            case .rateLimited: "The service is rate limiting this token. Try again shortly."
            case let .http(status, body): "The request failed (\(status)): \(body)"
            case .noChoices: "The service returned no answer."
            }
        }
    }

    /// `maxTokens` bounds the answer: enough for a lookup unless said otherwise.
    /// `toolsAllowed` false keeps the tools listed but lets the model only
    /// answer, for the last request of a conversation that has used them.
    static func chat(
        settings: AISettings,
        messages: [ChatMessage],
        tools: [[String: Any]] = [],
        toolsAllowed: Bool = true,
        jsonMode: Bool = false,
        maxTokens: Int = 700
    ) async throws -> ChatMessage {
        guard !settings.usesMock else { return MockAI.reply(to: messages) }
        guard let url = settings.chatURL else {
            throw APIError.invalidEndpoint(settings.endpoint)
        }

        let signature = "\(settings.endpoint)|\(settings.model)"
        var quirks = await RequestQuirkStore.shared.quirks(for: signature)

        // A service that rejects a parameter says which one, so drop or rename
        // it and try again rather than failing the lookup.
        for _ in 0...RequestQuirk.allCases.count {
            var request = URLRequest(url: url)
            request.httpMethod = "POST"
            // A longer answer takes longer to write.
            request.timeoutInterval = TimeInterval(max(45, maxTokens / 20))
            request.setValue("application/json", forHTTPHeaderField: "Content-Type")
            request.httpBody = try JSONSerialization.data(
                withJSONObject: settings.usesAnthropic
                    ? AnthropicMessages.body(
                        model: settings.model,
                        messages: messages,
                        tools: tools,
                        toolsAllowed: toolsAllowed,
                        maxTokens: maxTokens,
                        quirks: quirks
                    )
                    : body(
                        settings: settings,
                        messages: messages,
                        tools: toolsAllowed ? tools : [],
                        jsonMode: jsonMode,
                        maxTokens: maxTokens,
                        quirks: quirks
                    )
            )

            do {
                let data = try await send(request, settings: settings)
                let reply = try settings.usesAnthropic
                    ? AnthropicMessages.reply(from: data)
                    : JSONDecoder().decode(Completion.self, from: data).choices.first?.message
                guard let message = reply else { throw APIError.noChoices }
                return message
            } catch let APIError.http(status, body) where status == 400 {
                guard let quirk = RequestQuirk.named(inErrorBody: body) else {
                    throw APIError.http(status: status, body: body)
                }
                guard quirks.insert(quirk).inserted else {
                    // Refused over something already adjusted: what was
                    // remembered no longer fits, so the next request finds
                    // out afresh.
                    await RequestQuirkStore.shared.forget(signature)
                    throw APIError.http(status: status, body: body)
                }
                await RequestQuirkStore.shared.learn(quirk, for: signature)
            }
        }

        throw APIError.noChoices
    }

    static func body(
        settings: AISettings,
        messages: [ChatMessage],
        tools: [[String: Any]],
        jsonMode: Bool,
        maxTokens: Int = 700,
        quirks: Set<RequestQuirk>
    ) throws -> [String: Any] {
        var body: [String: Any] = [
            "model": settings.model,
            "messages": try JSONSerialization.jsonObject(with: JSONEncoder().encode(messages))
        ]
        body[quirks.contains(.completionTokens) ? "max_completion_tokens" : "max_tokens"] = maxTokens
        if !quirks.contains(.defaultTemperature) {
            body["temperature"] = 0.2
        }
        if quirks.contains(.noReasoning) {
            body["reasoning_effort"] = "none"
        }
        if !tools.isEmpty {
            body["tools"] = tools
            body["tool_choice"] = "auto"
        }
        if jsonMode {
            body["response_format"] = ["type": "json_object"]
        }
        return body
    }

    /// The model names the endpoint offers, sorted.
    static func models(settings: AISettings) async throws -> [String] {
        guard !settings.usesMock else { return MockAI.models }
        guard let url = settings.modelsURL else {
            throw APIError.invalidEndpoint(settings.endpoint)
        }

        var request = URLRequest(url: url)
        request.timeoutInterval = 20
        request.setValue("application/json", forHTTPHeaderField: "Accept")

        let data = try await send(request, settings: settings)
        let list = try JSONDecoder().decode(ModelList.self, from: data)
        return Set(list.data.map(\.id)).sorted()
    }

    private static func send(_ request: URLRequest, settings: AISettings) async throws -> Data {
        var request = request
        // Anthropic takes the token in a header of its own, with the API version.
        if settings.usesAnthropic {
            request.setValue(AnthropicMessages.version, forHTTPHeaderField: "anthropic-version")
        }
        if !settings.apiKey.isEmpty {
            if settings.usesAnthropic {
                request.setValue(settings.apiKey, forHTTPHeaderField: "x-api-key")
            } else {
                request.setValue("Bearer \(settings.apiKey)", forHTTPHeaderField: "Authorization")
            }
        }

        let (data, response) = try await URLSession.shared.data(for: request)
        if let http = response as? HTTPURLResponse, !(200...299).contains(http.statusCode) {
            if http.statusCode == 429 { throw APIError.rateLimited }
            throw APIError.http(
                status: http.statusCode,
                body: String(data: data, encoding: .utf8) ?? ""
            )
        }
        return data
    }

    private struct Completion: Decodable {
        struct Choice: Decodable { let message: ChatMessage }
        let choices: [Choice]
    }

    private struct ModelList: Decodable {
        struct Model: Decodable { let id: String }
        let data: [Model]
    }
}
