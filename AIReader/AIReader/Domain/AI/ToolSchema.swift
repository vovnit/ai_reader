import Foundation

/// The shape of a function tool with one string argument, as the API wants
/// it, and the reading of that argument out of a call.
enum ToolSchema {
    /// `choices`, when given, are the only values the argument may take.
    static func function(
        name: String,
        description: String,
        argument: String,
        argumentDescription: String,
        choices: [String] = []
    ) -> [String: Any] {
        var property: [String: Any] = ["type": "string", "description": argumentDescription]
        if !choices.isEmpty { property["enum"] = choices }
        return [
            "type": "function",
            "function": [
                "name": name,
                "description": description,
                "parameters": [
                    "type": "object",
                    "properties": [argument: property],
                    "required": [argument]
                ]
            ]
        ]
    }

    static func argument(_ name: String, in arguments: String) -> String? {
        guard let data = arguments.data(using: .utf8),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else { return nil }
        return object[name] as? String
    }
}
