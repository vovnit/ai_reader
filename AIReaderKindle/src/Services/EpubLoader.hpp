#pragma once

#include "../Domain/Books/BookDocument.hpp"
#include "../Domain/Books/EpubPackage.hpp"

#include <optional>
#include <string>

/// Reads an `.epub` where it lies: its metadata for the shelf, its chapters
/// for reading, and its cover for the list.
namespace EpubLoader {

struct Metadata {
    std::string title;
    std::string author;
    std::string language;
};

struct Failure {
    std::string message;
};

/// The metadata, or why the file could not be opened.
std::optional<Metadata> metadata(const std::string& path, std::string* error = nullptr);
/// The chapters. Without `withImages` the pictures are left out, which is
/// all a search needs and spares reading them out of the archive.
std::optional<BookDocument> load(const std::string& path, std::string* error = nullptr, bool withImages = true);
/// The cover image's bytes, if the book has one.
std::optional<std::string> cover(const std::string& path);

}  // namespace EpubLoader
