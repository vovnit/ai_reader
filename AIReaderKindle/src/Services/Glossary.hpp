#pragma once

#include "../Domain/Books/Book.hpp"
#include "../Domain/Reading/BookWords.hpp"
#include "DictionaryPacks.hpp"
#include "Settings.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

/// A book's offline glossary: a word list beside the other dictionaries that
/// the model's definitions are appended to, a batch at a time, and the
/// dictionary made of it — the same list DictionaryTool's `book_glossary.py`
/// writes, so one begun there is continued here.
namespace Glossary {

/// A book's glossary is the dictionary of this name.
std::string name(const Book& book);
/// The word list.
std::string path(const Book& book);
/// The forms the word list defines so far.
std::set<std::string> definedForms(const Book& book);
/// Form → definition for a batch, from the model. Throws `ChatApi::Error`.
std::map<std::string, std::string> define(const AiSettings& settings, const std::vector<BookWord>& words);
/// Appends definitions to the word list; safe from several threads at once.
void append(const Book& book, const AiSettings& settings, const std::map<std::string, std::string>& definitions);
/// Makes the word list the book's dictionary, in place of the one made before.
void install(DictionaryPacks& packs, const Book& book);

}  // namespace Glossary
