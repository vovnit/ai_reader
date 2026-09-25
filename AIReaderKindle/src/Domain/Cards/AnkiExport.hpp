#pragma once

#include "Card.hpp"

#include <string>
#include <vector>

/// Cards as the text file Anki imports: one note per line, fields a tab
/// apart, under the header lines newer Anki reads and older Anki skips.
/// The front is the word with its lemma and the sentence it was met in;
/// the back is the meaning it had there.
namespace AnkiExport {

std::string text(const std::vector<Card>& cards, const std::string& deck);

}  // namespace AnkiExport
