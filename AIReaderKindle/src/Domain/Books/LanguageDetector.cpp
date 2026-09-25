#include "LanguageDetector.hpp"

#include "../../Support/Text.hpp"

#include <glib.h>

#include <map>
#include <set>

namespace LanguageDetector {

namespace {

const std::map<std::string, std::set<std::string>> stopwords = {
    {"en", {"the", "of", "and", "to", "in", "is", "that", "it", "was", "he", "for", "on", "are", "with", "as",
            "his", "they", "at", "be", "this", "have", "from", "or", "had", "by", "not", "but", "she", "you", "were"}},
    {"fr", {"le", "la", "les", "des", "et", "une", "du", "que", "qui", "dans", "pas", "pour", "ne", "ce", "il",
            "elle", "je", "au", "sur", "avec", "est", "se", "son", "sa", "ses", "mais", "plus", "vous", "nous", "lui"}},
    {"de", {"der", "die", "das", "und", "ist", "nicht", "ich", "sie", "er", "es", "ein", "eine", "den", "dem",
            "zu", "mit", "sich", "auf", "für", "von", "auch", "dass", "wie", "als", "aber", "nach", "bei", "war", "wir"}},
    {"es", {"el", "los", "las", "del", "y", "que", "en", "un", "una", "es", "no", "se", "por", "con", "para", "su",
            "al", "lo", "como", "más", "pero", "sus", "ya", "este", "porque", "muy", "había", "era", "cuando"}},
    {"it", {"il", "di", "che", "è", "un", "una", "non", "per", "in", "del", "della", "con", "si", "gli", "ma",
            "come", "più", "anche", "lo", "se", "nel", "sono", "era", "questo", "aveva", "della", "alla", "dei"}},
    {"pt", {"o", "os", "as", "do", "da", "dos", "das", "e", "que", "um", "uma", "em", "não", "se", "com", "para",
            "por", "mais", "mas", "como", "ele", "ela", "seu", "sua", "foi", "era", "muito", "ao", "isso"}},
    {"ru", {"и", "в", "не", "на", "что", "он", "с", "я", "как", "а", "то", "все", "она", "так", "его", "но", "да",
            "ты", "к", "у", "же", "вы", "за", "бы", "по", "мне", "было", "вот", "от", "меня", "это"}},
    {"nl", {"het", "een", "en", "van", "ik", "te", "dat", "die", "is", "niet", "op", "zijn", "hij", "met", "als",
            "voor", "er", "maar", "om", "ook", "aan", "dan", "was", "had", "ze", "bij", "naar", "wat", "nog"}},
};

/// A few thousand characters from the middle: front matter is often in
/// another language.
std::string sample(const std::vector<PlainText>& chapters) {
    std::string whole;
    for (const auto& chapter : chapters) {
        whole += chapter.text;
        whole += '\n';
        if (whole.size() > 200000) break;
    }
    if (whole.size() < 200) return "";
    size_t start = whole.size() / 3;
    while (start < whole.size() && (static_cast<unsigned char>(whole[start]) & 0xC0) == 0x80) ++start;
    size_t length = std::min<size_t>(6000, whole.size() - start);
    while (start + length < whole.size() && (static_cast<unsigned char>(whole[start + length]) & 0xC0) == 0x80) ++length;
    return whole.substr(start, length);
}

}  // namespace

std::string detect(const std::vector<PlainText>& chapters) {
    std::string text = Text::lower(sample(chapters));
    if (text.empty()) return "";

    std::map<std::string, int> hits;
    int words = 0;
    std::string word;
    auto flush = [&] {
        if (word.empty()) return;
        ++words;
        for (const auto& language : stopwords) {
            if (language.second.count(word)) ++hits[language.first];
        }
        word.clear();
    };
    const char* p = text.c_str();
    const char* end = p + text.size();
    for (; p < end; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (g_unichar_isalpha(c)) word.append(p, g_utf8_next_char(p) - p);
        else flush();
    }
    flush();
    if (words < 30) return "";

    std::string best;
    int bestHits = 0, secondHits = 0;
    for (const auto& language : hits) {
        if (language.second > bestHits) {
            secondHits = bestHits;
            bestHits = language.second;
            best = language.first;
        } else if (language.second > secondHits) {
            secondHits = language.second;
        }
    }
    // Stand out clearly, or say nothing.
    if (bestHits * 10 < words || bestHits < secondHits * 3 / 2) return "";
    return best;
}

std::string code(const std::string& declared) {
    std::string text = Text::lower(Text::trim(declared));
    auto separator = text.find_first_of("-_");
    if (separator != std::string::npos) text = text.substr(0, separator);
    static const std::map<std::string, std::string> threeLetter = {
        {"fre", "fr"}, {"fra", "fr"}, {"eng", "en"}, {"ger", "de"}, {"deu", "de"}, {"spa", "es"},
        {"ita", "it"}, {"por", "pt"}, {"rus", "ru"}, {"dut", "nl"}, {"nld", "nl"},
    };
    auto known = threeLetter.find(text);
    return known == threeLetter.end() ? text : known->second;
}

}  // namespace LanguageDetector
