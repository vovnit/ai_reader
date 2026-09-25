import Foundation

/// The shape of a function tool with one string argument, as the API wants
/// it, and the reading of that argument out of a call.
enum ToolSchema {
    static func function(
        name: String,
        description: String,
        argument: String,
        argumentDescription: String
    ) -> [String: Any] {
        [
            "type": "function",
            "function": [
                "name": name,
                "description": description,
                "parameters": [
                    "type": "object",
                    "properties": [
                        argument: ["type": "string", "description": argumentDescription]
                    ],
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
