import Foundation
import Security

/// A tiny wrapper over the keychain's generic password items, for the few
/// secrets the app holds. Items are filed under the app's identifier and access
/// group so the extension finds the same ones.
///
/// A missing item and an item holding an empty string are different things: the
/// first means nothing was ever stored, the second that the value was
/// deliberately cleared.
enum Keychain {

    static func string(forKey key: String) -> String? {
        var query = query(forKey: key)
        query[kSecReturnData as String] = true
        query[kSecMatchLimit as String] = kSecMatchLimitOne

        var item: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess,
              let data = item as? Data
        else { return nil }
        return String(data: data, encoding: .utf8)
    }

    static func set(_ value: String, forKey key: String) {
        let data = Data(value.utf8)
        let query = query(forKey: key)

        let updated = SecItemUpdate(
            query as CFDictionary,
            [kSecValueData as String: data] as CFDictionary
        )
        guard updated == errSecItemNotFound else { return }

        var item = query
        item[kSecValueData as String] = data
        item[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlock
        _ = SecItemAdd(item as CFDictionary, nil)
    }

    static func removeValue(forKey key: String) {
        _ = SecItemDelete(query(forKey: key) as CFDictionary)
    }

    private static func query(forKey key: String) -> [String: Any] {
        [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: AppGroup.appBundleID,
            kSecAttrAccessGroup as String: AppGroup.keychainAccessGroup,
            kSecAttrAccount as String: key
        ]
    }
}
