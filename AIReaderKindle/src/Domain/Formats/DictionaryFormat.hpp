#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

/// One article, in the shape every reader produces and the writer consumes.
struct DictionaryImportEntry {
    std::string headword;
    std::string partOfSpeech;
    std::vector<std::string> senses;
};

/// What a dictionary file says about itself.
struct DictionaryImportInfo {
    std::string name;
    std::string targetLanguage;
    std::string definitionLanguage;
};

using DictionaryEntrySink = std::function<void(const DictionaryImportEntry&)>;

/// The dictionary file formats the app can read.
enum class DictionaryFormat {
    /// A pack this app already understands, added as it is.
    Native,
    /// One headword and its definition per line, separated by tabs or commas.
    Delimited,
    /// The open XDXF XML format.
    Xdxf,
    /// ABBYY Lingvo's DSL, plain or gzipped.
    Dsl,
    /// StarDict: an `.ifo` describing sibling `.idx` and `.dict` files.
    StarDict,
};

const char* dictionaryFormatLabel(DictionaryFormat format);

/// One dictionary to import: the file that names it, plus the files that
/// belong with it. Only StarDict spreads itself over several files.
struct DictionarySource {
    DictionaryFormat format;
    std::string main;
    std::vector<std::string> companions;

    /// The companion whose name ends in `suffix`, ignoring a trailing `.dz`.
    std::optional<std::string> companion(const std::string& suffix) const;
    /// The name to fall back on when the file itself carries none.
    std::string defaultName() const;
};

namespace DictionaryFormats {

/// Sorts a set of files into the dictionaries they make up. StarDict's
/// `.idx` and `.dict` files join the `.ifo` they sit beside.
std::vector<DictionarySource> sources(const std::vector<std::string>& paths);

/// The format of a single file, or nothing when it is a companion or unknown.
std::optional<DictionaryFormat> of(const std::string& path);

/// The file name without its extension, seeing through `.dz` and `.gz`.
std::string stem(const std::string& path);

}  // namespace DictionaryFormats

/// Turns whatever a dictionary calls its language into a two-letter code.
namespace LanguageName {

std::string code(const std::string& text);

}  // namespace LanguageName
