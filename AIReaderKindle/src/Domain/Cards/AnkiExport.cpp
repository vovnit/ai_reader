#include "AnkiExport.hpp"

namespace AnkiExport {

namespace {

/// Fields are read as HTML, so what would pass for markup is escaped, and
/// a field must stay on its line.
std::string field(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '\t': case '\n': case '\r': out += ' '; break;
        default: out += c;
        }
    }
    return out;
}

std::string line(const std::string& text) {
    std::string out;
    for (char c : text) out += (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
    return out;
}

}  // namespace

std::string text(const std::vector<Card>& cards, const std::string& deck) {
    std::string out = "#separator:tab\n#html:true\n#deck:" + line(deck) + "\n";
    for (const auto& card : cards) {
        std::string front = "<b>" + field(card.front) + "</b>";
        if (!card.lemma.empty()) front += " (" + field(card.lemma) + ")";
        if (!card.sentence.empty()) front += "<br><i>" + field(card.sentence) + "</i>";
        out += front + "\t" + field(card.back) + "\n";
    }
    return out;
}

}  // namespace AnkiExport
