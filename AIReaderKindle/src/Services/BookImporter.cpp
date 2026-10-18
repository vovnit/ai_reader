#include "BookImporter.hpp"

#include "EpubLoader.hpp"
#include "EpubWriter.hpp"

#include "../Domain/Books/Fb2Book.hpp"
#include "../Domain/Books/HtmlBook.hpp"
#include "../Domain/Books/TextBook.hpp"
#include "../Support/Charset.hpp"
#include "../Support/Files.hpp"
#include "../Support/Text.hpp"
#include "../Support/ZipArchive.hpp"

#include <glib.h>

#include <cstring>
#include <optional>
#include <stdexcept>
#include <utility>

namespace BookImporter {

namespace {

// Name endings and the format each names. ".fb2.zip" is one ending, so the
// rest of the name is the title, and any ZIP is looked into for an FB2, or
// else an EPUB.
const std::pair<const char*, const char*> endings[] = {
    {".fb2", "fb2"}, {".fb2.zip", "fb2.zip"}, {".fbz", "fb2.zip"}, {".zip", "fb2.zip"},
    {".txt", "txt"}, {".html", "html"}, {".htm", "html"}, {".xhtml", "html"},
};

struct Format {
    /// fb2, fb2.zip, txt or html; empty for a name this does not know.
    std::string kind;
    /// The name without its ending.
    std::string title;
};

/// The endings are ASCII, so comparing them without regard to ASCII case is
/// lowercasing the name and comparing, as the other apps do.
bool endsWith(const std::string& name, const char* ending) {
    size_t length = std::strlen(ending);
    return name.size() >= length && g_ascii_strcasecmp(name.c_str() + name.size() - length, ending) == 0;
}

Format format(const std::string& path) {
    std::string name = Files::baseName(path);
    for (const auto& [ending, kind] : endings) {
        if (endsWith(name, ending)) return {kind, name.substr(0, name.size() - std::strlen(ending))};
    }
    return {};
}

/// A file's text in the encoding it names, if it names one.
std::string declaredText(const std::string& bytes) {
    return Charset::decode(bytes, Charset::declared(bytes));
}

/// Whether a ZIP entry is an FB2, and not the resource fork a Mac adds
/// beside a file ("__MACOSX/", "._"), which ends the same.
bool isFb2Entry(const std::string& entry) {
    std::string base = entry.substr(entry.rfind('/') + 1);
    return endsWith(entry, ".fb2") && !Text::startsWith(entry, "__MACOSX/") && !Text::startsWith(base, "._");
}

/// The first FB2 the ZIP lists.
std::optional<std::string> fb2Entry(const ZipArchive& zip) {
    for (const auto& entry : zip.paths()) {
        if (isFb2Entry(entry)) return entry;
    }
    return std::nullopt;
}

/// An EPUB is a ZIP too, and one renamed .zip is still a book.
bool isEpub(const ZipArchive& zip) {
    return !fb2Entry(zip) && zip.find("META-INF/container.xml");
}

/// The bytes of the first FB2 in a ZIP, or nothing for a ZIP that is an EPUB.
std::optional<std::string> zippedFb2(const std::string& bytes) {
    std::string error;
    auto zip = ZipArchive::read(bytes, &error);
    if (!zip) throw std::runtime_error(error);
    if (isEpub(*zip)) return std::nullopt;
    auto entry = fb2Entry(*zip);
    if (!entry) throw std::runtime_error("The ZIP holds no FB2 book.");
    auto fb2 = zip->contents(*entry);
    if (!fb2) throw std::runtime_error("The FB2 book in the ZIP cannot be read.");
    return fb2;
}

/// Puts an EPUB in `folder` as `LibraryFeature::add` puts one there, once it
/// has been read, under a name ending .epub so the shelf finds it.
std::string shelveEpub(const std::string& path, const std::string& folder, const std::string& name, std::string* error) {
    std::string reason;
    if (!EpubLoader::metadata(path, &reason)) {
        if (error) *error = reason.empty() ? "Not an EPUB this app can read." : reason;
        return "";
    }
    std::string destination = EpubWriter::destination(folder, name, EpubWriter::identifierOf(path));
    if (!Files::exists(destination) && !Files::copy(path, destination)) {
        if (error) *error = "Could not copy the book into " + folder + ".";
        return "";
    }
    return destination;
}

/// The EPUB of a book read from a file of the given kind; the file's own
/// bytes name it.
std::string draftEpub(const BookDraft& draft, const std::string& kind, const std::string& file) {
    if (draft.chapters.empty()) throw std::runtime_error("The file has no text to read.");
    EpubBuilder::Metadata metadata;
    metadata.title = draft.title;
    metadata.author = draft.author;
    metadata.language = draft.language.empty() ? EpubWriter::proseLanguage(draft.chapters) : draft.language;
    if (metadata.language.empty()) metadata.language = "und";
    metadata.identifier = EpubWriter::identifier(kind, file);
    return EpubWriter::epub(metadata, draft.chapters);
}

}  // namespace

bool reads(const std::string& path) {
    return !format(path).kind.empty();
}

std::string epub(const std::string& file, const std::string& path) {
    Format named = format(path);
    if (named.kind == "fb2") return draftEpub(Fb2Book::read(declaredText(file), named.title), "fb2", file);
    if (named.kind == "fb2.zip") {
        auto fb2 = zippedFb2(file);
        return fb2 ? draftEpub(Fb2Book::read(declaredText(*fb2), named.title), "fb2", file) : file;
    }
    // A text file names no encoding; Charset tells it from the bytes.
    if (named.kind == "txt") return draftEpub(TextBook::read(Charset::decode(file), named.title), "txt", file);
    if (named.kind == "html") return draftEpub(HtmlBook::read(declaredText(file), named.title), "html", file);
    throw std::runtime_error("Not a book this app can read.");
}

std::string convert(const std::string& path, const std::string& folder, std::string* error) {
    Format named = format(path);
    auto zip = named.kind == "fb2.zip" ? ZipArchive::open(path) : std::nullopt;
    if (zip && isEpub(*zip)) return shelveEpub(path, folder, named.title, error);
    std::string kind = named.kind == "fb2.zip" ? "fb2" : named.kind;
    return EpubWriter::write(path, folder, named.title, kind, [&path](const std::string& file) { return epub(file, path); }, error);
}

}  // namespace BookImporter
