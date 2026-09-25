#pragma once

#include "Common/Screen.hpp"
#include "Domain/Dictionary/DictionaryLookup.hpp"

#include <string>
#include <vector>

/// The dictionary's own articles for a lemma, as the explanation drew on them.
class EntryView : public Screen {
public:
    EntryView(Navigator& navigator, const std::string& lemma, const std::vector<DictionaryLookup::Article>& articles);
};
