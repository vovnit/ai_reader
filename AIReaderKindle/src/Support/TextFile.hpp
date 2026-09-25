#pragma once

#include <optional>
#include <string>
#include <vector>

/// Reads a text file whose encoding is only known from its first bytes.
/// Dictionary sources arrive as UTF-8, as UTF-16 (Lingvo's habit), and
/// gzipped (the `.dz` files that ship beside StarDict and DSL dictionaries).
namespace TextFile {

std::optional<std::string> text(const std::string& path);
std::optional<std::vector<std::string>> lines(const std::string& path);
/// The bytes as UTF-8, whatever they started as.
std::string decode(const std::string& data);

}  // namespace TextFile
