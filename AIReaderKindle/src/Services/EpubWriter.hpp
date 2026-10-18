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

/// The `dc:identifier` of the EPUB at `path`, or empty.
std::string identifierOf(const std::string& path);

/// Where the book of this identifier goes in `folder`: `<name>.epub`, named
/// as a book fetched from the sync folder is, or `<name> (2).epub` and on
/// when another book has the name. One already there with this identifier
/// is this book, made before.
std::string destination(const std::string& folder, const std::string& name, const std::string& identifier);

/// Writes the EPUB `make` makes of the bytes of the file at `path`, a file
/// of the `kind` its identifier names, into `folder` at its `destination`,
/// and returns its path. Empty, with `error` set, when the file cannot be
/// read, `make` throws, or nothing is written.
std::string write(const std::string& path, const std::string& folder, const std::string& name, const std::string& kind,
                  const std::function<std::string(const std::string& file)>& make, std::string* error);

}  // namespace EpubWriter
