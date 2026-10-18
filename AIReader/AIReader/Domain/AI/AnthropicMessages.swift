import Foundation

/// Anthropic's Messages API, which its endpoint speaks in place of chat
/// completions: the request made from the same messages and tools, and the
/// reply read back into a `ChatMessage`. A reply keeps its content blocks as
/// they came, thinking included, and they go back unchanged in the requests
/// that follow: the API checks that they do.
enum AnthropicMessages {
    enum ReplyError: LocalizedError {
        case declined

        var errorDescription: String? { "The model declined to answer." }
    }

    /// The API version every request names.
    static let version = "2023-06-01"

    /// `toolsAllowed` is false for the request that must be answered in words.
    static func body(
        model: String,
        messages: [ChatMessage],
        tools: [[String: Any]],
        toolsAllowed: Bool,
        maxTokens: Int,
        quirks: Set<RequestQuirk>
    ) -> [String: Any] {
        var body: [String: Any] = ["model": model, "max_tokens": maxTokens]
        let system = messages.filter { $0.role == "system" }.compactMap(\.content).filter { !$0.isEmpty }
        if !system.isEmpty {
            body["system"] = system.joined(separator: "\n\n")
        }
        body["messages"] = turns(messages)
        // Newer models think before answering, and the thinking counts
        // against `max_tokens`: a lookup wants little of it.
        if !quirks.contains(.noEffort) {
            body["output_config"] = ["effort": "low"]
        }
        if !tools.isEmpty {
            body["tools"] = tools.map { tool -> [String: Any] in
                let function = tool["function"] as? [String: Any] ?? [:]
                return [
                    "name": function["name"] ?? "",
                    "description": function["description"] ?? "",
                    "input_schema": function["parameters"] ?? [:]
                ]
            }
            // A conversation that has called the tools must still list them,
            // so they are kept and only their use is forbidden.
            if !toolsAllowed {
                body["tool_choice"] = ["type": "none"]
            }
        }
        return body
    }

    /// The reply as a message, or nil when it holds none.
    static func reply(from data: Data) throws -> ChatMessage? {
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return nil }
        if json["stop_reason"] as? String == "refusal" { throw ReplyError.declined }
        guard let content = json["content"] as? [[String: Any]] else { return nil }

        let text = content.filter { $0["type"] as? String == "text" }.compactMap { $0["text"] as? String }.joined()
        let calls = content.filter { $0["type"] as? String == "tool_use" }.map { block in
            let input = block["input"] as? [String: Any] ?? [:]
            let arguments = (try? JSONSerialization.data(withJSONObject: input)).flatMap { String(data: $0, encoding: .utf8) }
            return ChatMessage.ToolCall(
                id: block["id"] as? String ?? "",
                function: .init(name: block["name"] as? String ?? "", arguments: arguments ?? "{}")
            )
        }
        return ChatMessage(
            role: "assistant",
            content: text.isEmpty ? nil : text,
            toolCalls: calls.isEmpty ? nil : calls,
            blocks: try JSONSerialization.data(withJSONObject: content)
        )
    }

    /// Tool results travel in a user turn and the roles must alternate, so
    /// neighbours of one role are joined.
    private static func turns(_ messages: [ChatMessage]) -> [[String: Any]] {
        var turns: [(role: String, content: [Any])] = []
        for message in messages where message.role != "system" {
            let role = message.role == "assistant" ? "assistant" : "user"
            let content = blocks(of: message)
            guard !content.isEmpty else { continue }
            if turns.last?.role == role {
                turns[turns.count - 1].content += content
            } else {
                turns.append((role, content))
            }
        }
        return turns.map { ["role": $0.role, "content": $0.content] }
    }

    private static func blocks(of message: ChatMessage) -> [Any] {
        if let blocks = message.blocks,
           let content = try? JSONSerialization.jsonObject(with: blocks) as? [Any] {
            return content
        }
        if let id = message.toolCallID {
            return [["type": "tool_result", "tool_use_id": id, "content": message.content ?? ""] as [String: Any]]
        }
        var blocks: [Any] = []
        if let text = message.content, !text.isEmpty {
            blocks.append(["type": "text", "text": text] as [String: Any])
        }
        for call in message.toolCalls ?? [] {
            let input = call.function.arguments.data(using: .utf8)
                .flatMap { try? JSONSerialization.jsonObject(with: $0) as? [String: Any] } ?? [:]
            blocks.append(["type": "tool_use", "id": call.id, "name": call.function.name, "input": input] as [String: Any])
        }
        return blocks
    }
}
