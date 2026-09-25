#include "RemoteBookName.hpp"

#include "../../Support/Text.hpp"

#include <cstring>
#include <set>

namespace RemoteBookName {

const char* const folder = "Books";

namespace {

std::string stem(const std::string& title, const std::string& author) {
    std::string trimmedAuthor = Text::trim(author);
    std::string raw = trimmedAuthor.empty() ? title : trimmedAuthor + " - " + title;

    // Forbidden and control characters become spaces, and runs of spaces one.
    std::string cleaned;
    for (char c : raw) {
        auto byte = static_cast<unsigned char>(c);
        bool blank = byte < 0x20 || byte == 0x7F || std::strchr("/\\:*?\"<>| ", c) != nullptr;
        if (!blank) cleaned += c;
        else if (!cleaned.empty() && cleaned.back() != ' ') cleaned += ' ';
    }

    // At most 120 characters, cut between UTF-8 sequences.
    size_t characters = 0;
    for (size_t i = 0; i < cleaned.size(); ++i) {
        if ((static_cast<unsigned char>(cleaned[i]) & 0xC0) == 0x80) continue;
        if (++characters > 120) {
            cleaned.resize(i);
            break;
        }
    }

    // Windows and FAT drop a trailing dot, and a leading one hides the file.
    while (!cleaned.empty() && (cleaned.back() == '.' || cleaned.back() == ' ')) cleaned.pop_back();
    size_t start = cleaned.find_first_not_of(". ");
    cleaned = start == std::string::npos ? "" : cleaned.substr(start);
    return cleaned.empty() ? "Book" : cleaned;
}

}  // namespace

std::string make(const std::string& title, const std::string& author, const std::vector<std::string>& taken) {
    std::set<std::string> lowered;
    for (const auto& name : taken) lowered.insert(Text::lower(name));
    std::string base = stem(title, author);
    std::string name = base + ".epub";
    for (int number = 2; lowered.count(Text::lower(name)); ++number) {
        name = base + " (" + std::to_string(number) + ").epub";
    }
    return name;
}

bool isBook(const std::string& name) {
    return Text::endsWith(Text::lower(name), ".epub") && !Text::startsWith(name, ".");
}

}  // namespace RemoteBookName
