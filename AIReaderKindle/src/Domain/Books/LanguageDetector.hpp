#pragma once

#include "HtmlText.hpp"

#include <string>
#include <vector>

/// Reads the language a book is written in off its prose, by how often the
/// commonest words of each language turn up. Coarse, but enough to tell a
/// French novel from the "en" its metadata claims.
namespace LanguageDetector {

/// A two-letter code, or empty when no language stands out.
std::string detect(const std::vector<PlainText>& chapters);

/// "fr-FR", "FR" and "fre" all become "fr".
std::string code(const std::string& declared);

}  // namespace LanguageDetector
