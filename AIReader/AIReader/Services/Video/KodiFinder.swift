import dnssd
import Foundation
import Network
import Synchronization

/// Finds a Kodi on the local network by the Bonjour service its web server
/// announces, so nobody has to know the TV's address. The result is the
/// box's `.local` name, which stays right when its address changes.
enum KodiFinder {
    private static let service = "_xbmc-jsonrpc-h._tcp"

    static func find(timeout: Duration = .seconds(8)) async -> PlayerAddress? {
        let browser = NWBrowser(for: .bonjour(type: service, domain: nil), using: .tcp)
        let finished = Mutex(false)
        return await withCheckedContinuation { continuation in
            @Sendable func finish(_ address: PlayerAddress?) {
                let first = finished.withLock { done in
                    defer { done = true }
                    return !done
                }
                guard first else { return }
                browser.cancel()
                continuation.resume(returning: address)
            }
            browser.browseResultsChangedHandler = { results, _ in
                for result in results {
                    if let address = resolve(result.endpoint) { finish(address) }
                }
            }
            browser.stateUpdateHandler = { state in
                if case .failed = state { finish(nil) }
            }
            browser.start(queue: .global())
            Task {
                try? await Task.sleep(for: timeout)
                finish(nil)
            }
        }
    }

    /// A Bonjour result names a service; its SRV record names the host.
    private static func resolve(_ endpoint: NWEndpoint) -> PlayerAddress? {
        guard case let .service(name, type, domain, _) = endpoint else { return nil }
        final class Answer {
            var address: PlayerAddress?
        }
        let answer = Answer()
        let context = Unmanaged.passUnretained(answer).toOpaque()
        var reference: DNSServiceRef?
        let started = DNSServiceResolve(&reference, 0, 0, name, type, domain, { _, _, _, error, _, host, port, _, _, context in
            guard error == kDNSServiceErr_NoError, let host, let context else { return }
            let answer = Unmanaged<Answer>.fromOpaque(context).takeUnretainedValue()
            answer.address = PlayerAddress(
                host: String(cString: host).trimmingCharacters(in: CharacterSet(charactersIn: ".")),
                port: Int(UInt16(bigEndian: port))
            )
        }, context)
        guard started == kDNSServiceErr_NoError, let reference else { return nil }
        defer { DNSServiceRefDeallocate(reference) }
        // Wait for one answer, but not forever.
        var descriptor = pollfd(fd: DNSServiceRefSockFD(reference), events: Int16(POLLIN), revents: 0)
        guard poll(&descriptor, 1, 3000) > 0 else { return nil }
        DNSServiceProcessResult(reference)
        return answer.address
    }
}
