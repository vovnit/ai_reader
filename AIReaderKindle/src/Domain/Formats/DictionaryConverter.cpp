#include "DictionaryConverter.hpp"

#include "../../Support/Files.hpp"
#include "DSLDictionaryReader.hpp"
#include "DelimitedDictionaryReader.hpp"
#include "StarDictReader.hpp"
#include "XDXFDictionaryReader.hpp"

namespace DictionaryConverter {

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry, std::string* error) {
    std::optional<DictionaryImportInfo> info;
    switch (source.format) {
    case DictionaryFormat::Native:
        if (error) *error = Files::baseName(source.main) + " is not a dictionary format this app converts.";
        return std::nullopt;
    case DictionaryFormat::Delimited: info = DelimitedDictionaryReader::read(source, entry); break;
    case DictionaryFormat::Xdxf: info = XDXFDictionaryReader::read(source, entry); break;
    case DictionaryFormat::Dsl: info = DSLDictionaryReader::read(source, entry); break;
    case DictionaryFormat::StarDict: return StarDictReader::read(source, entry, error);
    }
    if (!info && error) *error = Files::baseName(source.main) + " could not be read.";
    return info;
}

}  // namespace DictionaryConverter
