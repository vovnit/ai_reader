import Foundation

/// One message in a chat completion exchange.
struct ChatMessage: Codable, Equatable, Sendable {
    struct ToolCall: Codable, Equatable, Sendable {
        struct Function: Codable, Equatable, Sendable {
            var name: String
            /// A JSON object, encoded as a string, as the API returns it.
            var arguments: String
        }

        var id: String
        /// Services require this back on every call they made, even the ones
        /// that leave it out of their own reply.
        var type = "function"
        var function: Function
    }

    var role: String
    var content: String?
    var toolCalls: [ToolCall]?
    var toolCallID: String?

    enum CodingKeys: String, CodingKey {
        case role
        case content
        case toolCalls = "tool_calls"
        case toolCallID = "tool_call_id"
    }

    static func system(_ content: String) -> Self { Self(role: "system", content: content) }
    static func user(_ content: String) -> Self { Self(role: "user", content: content) }

    static func toolResult(_ content: String, callID: String) -> Self {
        Self(role: "tool", content: content, toolCallID: callID)
    }
}

extension ChatMessage.ToolCall {
    // Written by hand so a missing `type` in the reply falls back to the
    // default rather than failing the whole decode.
    init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(String.self, forKey: .id)
        type = try container.decodeIfPresent(String.self, forKey: .type) ?? "function"
        function = try container.decode(Function.self, forKey: .function)
    }
}
