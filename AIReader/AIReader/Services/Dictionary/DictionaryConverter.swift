import Foundation

/// Turns a dictionary in one of the formats readers hand around into the pack
/// this app searches.
enum DictionaryConverter {
    static func convert(_ source: DictionarySource, to destination: URL) throws -> DictionaryImportInfo {
        let writer = try DictionaryPackWriter(creating: destination)
        let info = try read(source) { try writer.add($0) }
        try writer.finish(info)
        return info
    }

    private static func read(
        _ source: DictionarySource,
        entry: (DictionaryImportEntry) throws -> Void
    ) throws -> DictionaryImportInfo {
        switch source.format {
        case .native:
            throw DictionaryReadError.unrecognized(source.main.lastPathComponent)
        case .delimited:
            try DelimitedDictionaryReader.read(source, entry: entry)
        case .xdxf:
            try XDXFDictionaryReader.read(source, entry: entry)
        case .dsl:
            try DSLDictionaryReader.read(source, entry: entry)
        case .starDict:
            try StarDictReader.read(source, entry: entry)
        }
    }
}
