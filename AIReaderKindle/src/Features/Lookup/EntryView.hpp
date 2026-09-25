#pragma once

#include "../../Domain/Dictionary/DictionaryLookup.hpp"

#include <string>
#include <vector>

class Navigator;

/// A dictionary entry as the dictionary has it: every article under the
/// headword, sense by sense, and which dictionary each came from.
namespace EntryView {

void open(Navigator& navigator, const std::string& lemma, const std::vector<DictionaryLookup::Article>& articles);

}  // namespace EntryView
