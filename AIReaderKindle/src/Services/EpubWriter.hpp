#pragma once

#include "../Domain/Books/EpubBuilder.hpp"

#include <functional>
#include <string>
#include <vector>

/// Writes a book read from another format — a PDF (`PdfImporter`), an FB2,
/// a text or an HTML file (`BookImporter`) — as the EPUB the library keeps,
/// so it is read, searched and synced like any other. The same as the web
/// app's `EpubWriter.js` and the iOS app's `EPUBWriter.swift`, so the same
/// file makes a book with the same identifier on every device.
namespace EpubWriter {

/// `urn:aireader:<kind>:<FNV-1a 64 of the file's bytes>`, `kind` being pdf,
/// fb2, txt or html.
std::string identifier(const std::string& kind, const std::string& file);

/// The language the chapters' prose is in, or empty when none stands out.
std::string proseLanguage(const std::vector<ChapterDraft>& chapters);

/// The EPUB of the chapters, stamped with the time it is made.
std::string epub(EpubBuilder::Metadata metadata, const std::vector<ChapterDraft>& chapters);

/// Writes the EPUB `make` makes of the bytes of the file at `path` into
/// `folder` as `<name>.epub`, and returns its path. An EPUB of that name
/// already there is taken to be this one, made before. Empty, with `error`
/// set, when the file cannot be read, `make` throws, or nothing is written.
std::string write(const std::string& path, const std::string& folder, const std::string& name,
                  const std::function<std::string(const std::string& file)>& make, std::string* error);

}  // namespace EpubWriter
