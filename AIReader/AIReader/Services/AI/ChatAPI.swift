import Foundation

/// Chat completions and the model list, against any OpenAI-compatible service.
/// Request bodies are assembled as plain JSON so tool schemas stay readable.
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

    static func chat(
        settings: AISettings,
        messages: [ChatMessage],
        tools: [[String: Any]] = [],
        jsonMode: Bool = false
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
            request.timeoutInterval = 45
            request.setValue("application/json", forHTTPHeaderField: "Content-Type")
            request.httpBody = try JSONSerialization.data(
                withJSONObject: body(
                    settings: settings,
                    messages: messages,
                    tools: tools,
                    jsonMode: jsonMode,
                    quirks: quirks
                )
            )

            do {
                let data = try await send(request, apiKey: settings.apiKey)
                guard let message = try JSONDecoder().decode(Completion.self, from: data)
                    .choices.first?.message
                else { throw APIError.noChoices }
                return message
            } catch let APIError.http(status, body) where status == 400 {
                guard let quirk = RequestQuirk.named(inErrorBody: body),
                      quirks.insert(quirk).inserted
                else { throw APIError.http(status: status, body: body) }
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
        quirks: Set<RequestQuirk>
    ) throws -> [String: Any] {
        var body: [String: Any] = [
            "model": settings.model,
            "messages": try JSONSerialization.jsonObject(with: JSONEncoder().encode(messages))
        ]
        body[quirks.contains(.completionTokens) ? "max_completion_tokens" : "max_tokens"] = 700
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

        let data = try await send(request, apiKey: settings.apiKey)
        let list = try JSONDecoder().decode(ModelList.self, from: data)
        return Set(list.data.map(\.id)).sorted()
    }

    private static func send(_ request: URLRequest, apiKey: String) async throws -> Data {
        var request = request
        if !apiKey.isEmpty {
            request.setValue("Bearer \(apiKey)", forHTTPHeaderField: "Authorization")
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
