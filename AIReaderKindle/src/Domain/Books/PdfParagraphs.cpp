#include "PdfParagraphs.hpp"

#include <glib.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace PdfParagraphs {

namespace {

/// The size most of the book's text is set in, and how far apart its
/// lines usually are.
struct Measure {
    double size = 10;
    double gap = 12;
};

bool isBody(const PdfLine& line, const Measure& measure) {
    return std::abs(line.size - measure.size) < measure.size * 0.15;
}

bool isHeading(const PdfLine& line, const Measure& measure) {
    return line.size > measure.size * 1.2;
}

Measure measureOf(const std::vector<std::vector<PdfLine>>& pages) {
    Measure measure;
    // The size that most of the letters are, not most of the lines.
    std::map<double, size_t> letters;
    size_t total = 0;
    for (const auto& page : pages) {
        for (const auto& line : page) {
            letters[std::round(line.size * 10) / 10] += line.text.size();
            total += line.text.size();
        }
    }
    size_t counted = 0;
    for (const auto& entry : letters) {
        counted += entry.second;
        if (counted * 2 >= total) {
            measure.size = entry.first;
            break;
        }
    }
    std::vector<double> gaps;
    for (const auto& page : pages) {
        for (size_t i = 1; i < page.size(); ++i) {
            double gap = page[i].y - page[i - 1].y;
            if (isBody(page[i], measure) && isBody(page[i - 1], measure) && gap > 0 && gap < measure.size * 3) gaps.push_back(gap);
        }
    }
    if (gaps.empty()) {
        measure.gap = measure.size * 1.2;
    } else {
        std::nth_element(gaps.begin(), gaps.begin() + gaps.size() / 2, gaps.end());
        measure.gap = gaps[gaps.size() / 2];
    }
    return measure;
}

/// A line as it is compared with other pages' lines: in lower case, its
/// numbers and extra spaces gone.
std::string normalized(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (g_ascii_isdigit(c)) continue;
        if (g_ascii_isspace(c)) {
            if (!out.empty() && out.back() != ' ') out += ' ';
            continue;
        }
        out += g_ascii_tolower(c);
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

size_t words(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != ' ' && (i == 0 || text[i - 1] == ' ')) ++count;
    }
    return count;
}

/// A page number: digits or a roman numeral, perhaps between dashes.
bool isPageNumber(const std::string& text) {
    std::string bare;
    for (const char* p = text.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (c == ' ' || c == '-' || c == 0x2013 || c == 0x2014 || c == '|' || c == '.' || c == 0xB7) continue;
        if (c > 0x7F) return false;
        bare += static_cast<char>(c);
    }
    if (bare.empty() || bare.size() > 7) return false;
    bool digits = std::all_of(bare.begin(), bare.end(), [](char c) { return g_ascii_isdigit(c); });
    bool roman = std::all_of(bare.begin(), bare.end(), [](char c) { return std::string("ivxlcdmIVXLCDM").find(c) != std::string::npos; });
    return digits || roman;
}

/// Whether a paragraph ends as a sentence does, rather than being cut.
bool finished(const std::string& text) {
    if (text.empty()) return false;
    gunichar last = g_utf8_get_char(g_utf8_prev_char(text.c_str() + text.size()));
    if (last < 0x80) return std::string(".!?:;\")]").find(static_cast<char>(last)) != std::string::npos;
    return last == 0x2026 || last == 0xBB || last == 0x201D;
}

bool startsLowercase(const std::string& text) {
    return !text.empty() && g_unichar_islower(g_utf8_get_char(text.c_str()));
}

bool startsLetter(const std::string& text) {
    return !text.empty() && g_unichar_isalpha(g_utf8_get_char(text.c_str()));
}

/// Adds a line to its paragraph; a word broken with a hyphen at the line's
/// end is mended.
void join(std::string& paragraph, const std::string& line) {
    for (const std::string hyphen : {"-", "‐", "­"}) {
        bool soft = hyphen == "­";
        if (paragraph.size() >= hyphen.size() && paragraph.compare(paragraph.size() - hyphen.size(), hyphen.size(), hyphen) == 0
            && (soft || startsLowercase(line))) {
            paragraph.erase(paragraph.size() - hyphen.size());
            paragraph += line;
            return;
        }
    }
    paragraph += ' ' + line;
}

/// The text with single spaces, and without the object replacement
/// character a picture leaves in some PDFs' text, which the reader keeps
/// for its own pictures.
std::string collapsed(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (text.compare(i, 3, "\uFFFC") == 0) {
            i += 2;
            continue;
        }
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
        out += c;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

/// Nothing but whitespace, a no-break space included.
bool isBlank(const std::string& text) {
    for (const char* p = text.c_str(); *p; p = g_utf8_next_char(p)) {
        if (!g_unichar_isspace(g_utf8_get_char(p))) return false;
    }
    return true;
}

/// Where the text block starts, or ends: the outermost place at least two
/// body lines agree on, so one stray line cannot move it; the outermost
/// line when no two agree.
double edge(const std::vector<PdfLine>& lines, const Measure& measure, bool left) {
    std::map<long, int> counts;
    double outermost = left ? HUGE_VAL : -HUGE_VAL;
    for (const auto& line : lines) {
        if (!isBody(line, measure)) continue;
        double x = left ? line.left : line.right;
        ++counts[std::lround(x)];
        outermost = left ? std::min(outermost, x) : std::max(outermost, x);
    }
    if (left) {
        for (auto it = counts.begin(); it != counts.end(); ++it) if (it->second >= 2) return it->first;
    } else {
        for (auto it = counts.rbegin(); it != counts.rend(); ++it) if (it->second >= 2) return it->first;
    }
    return outermost;
}

/// The lines nearest the top and the foot of a page, outermost first, each
/// with how far it stands from the next line in: where running heads and
/// page numbers are.
std::vector<std::pair<size_t, double>> edges(const std::vector<PdfLine>& lines) {
    std::vector<size_t> order(lines.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return lines[a].y < lines[b].y; });
    std::vector<std::pair<size_t, double>> found;
    size_t count = order.size();
    for (size_t k = 0; k < 2 && k < count; ++k) {
        found.emplace_back(order[k], k + 1 < count ? lines[order[k + 1]].y - lines[order[k]].y : HUGE_VAL);
    }
    for (size_t k = 0; k < 2 && k + 2 < count; ++k) {
        size_t at = count - 1 - k;
        found.emplace_back(order[at], lines[order[at]].y - lines[order[at - 1]].y);
    }
    return found;
}

}  // namespace

std::vector<PdfParagraph> read(const std::vector<std::vector<PdfLine>>& pages) {
    const Measure measure = measureOf(pages);
    // A line repeated at the top or foot of three pages or more, and set
    // apart from the text, is a running head.
    std::map<std::string, int> repeats;
    for (const auto& page : pages) {
        std::set<std::string> seen;
        for (const auto& edge : edges(page)) {
            std::string key = normalized(page[edge.first].text);
            if (!key.empty() && seen.insert(key).second) ++repeats[key];
        }
    }
    auto isFurniture = [&](const PdfLine& line, double apart) {
        if (isHeading(line, measure)) return false;
        std::string key = normalized(line.text);
        return isPageNumber(line.text) || (repeats[key] >= 3 && apart > measure.gap * 1.3 && words(key) <= 8);
    };

    std::vector<PdfParagraph> found;
    for (size_t number = 0; number < pages.size(); ++number) {
        const auto& all = pages[number];
        std::vector<bool> dropped(all.size(), false);
        auto outer = edges(all);
        // The top two, then the foot two: each stops at the first line that
        // is the book's own.
        for (size_t k = 0; k < outer.size() && k < 2 && isFurniture(all[outer[k].first], outer[k].second); ++k) dropped[outer[k].first] = true;
        for (size_t k = 2; k < outer.size() && isFurniture(all[outer[k].first], outer[k].second); ++k) dropped[outer[k].first] = true;
        std::vector<PdfLine> lines;
        for (size_t i = 0; i < all.size(); ++i) {
            if (!dropped[i]) lines.push_back(all[i]);
        }
        double left = edge(lines, measure, true);
        double right = edge(lines, measure, false);
        for (size_t i = 0; i < lines.size(); ++i) {
            const PdfLine& line = lines[i];
            bool heading = isHeading(line, measure);
            bool starts;
            if (i == 0) {
                // A paragraph the page break cut goes on, unindented, in lower
                // case — or with any word, when it was long and unfinished.
                const PdfParagraph* open = found.empty() || found.back().heading || finished(found.back().text) ? nullptr : &found.back();
                bool indented = line.left - left > measure.size * 0.8;
                starts = !open || heading || indented
                    || !(startsLowercase(line.text) || (open->text.size() >= 100 && startsLetter(line.text)));
            } else {
                const PdfLine& above = lines[i - 1];
                double gap = line.y - above.y;
                if (heading && isHeading(above, measure)) {
                    starts = gap > line.size * 2;
                } else {
                    starts = heading != isHeading(above, measure)
                        || gap > measure.gap * 1.6 || gap < -measure.gap * 0.5
                        || line.left - above.left > measure.size * 0.8
                        // Two one-line paragraphs, both indented, the first a whole sentence.
                        || (line.left - left > measure.size * 0.8 && std::abs(line.left - above.left) <= measure.size * 0.8 && finished(above.text))
                        || (above.right < right - measure.size * 2.5 && finished(above.text));
                }
            }
            if (starts) found.push_back({line.text, heading, static_cast<int>(number)});
            else join(found.back().text, line.text);
        }
    }
    for (auto& paragraph : found) paragraph.text = collapsed(paragraph.text);
    found.erase(std::remove_if(found.begin(), found.end(), [](const PdfParagraph& paragraph) { return isBlank(paragraph.text); }), found.end());
    return found;
}

}  // namespace PdfParagraphs
