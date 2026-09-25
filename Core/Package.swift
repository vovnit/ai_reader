// swift-tools-version: 6.0
import PackageDescription

// C++ both apps compile: the Kindle app lists these sources in its
// meson.build, the iOS app imports them through Swift's C++ interop. Only the
// standard library is allowed here, so it builds for both.
let package = Package(
    name: "Core",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "AIReaderCore", targets: ["AIReaderCore"]),
    ],
    targets: [
        // Headers sit next to their sources, the way they do in the Kindle
        // app, and are included by their path from here: "Support/Json.hpp".
        .target(name: "AIReaderCore", publicHeadersPath: "."),
    ],
    cxxLanguageStandard: .cxx17
)
