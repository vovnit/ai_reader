#include "PdfDocument.hpp"

#include "PdfEncodings.hpp"
#include "PdfFilters.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace {

bool isSpace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\0';
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

}  // namespace

PdfDocument::PdfDocument(std::string data) : data_(std::move(data)) {
    if (data_.find("%PDF") > 1024) throw std::runtime_error("The file is not a PDF.");
    scan();
    if (!get(trailer_, "Encrypt").isNull()) {
        throw std::runtime_error("The PDF is encrypted, and its text cannot be read here.");
    }
    catalog_ = get(trailer_, "Root");
    if (!catalog_.is(PdfObject::Type::Dictionary)) {
        // No trailer to name it: the catalog is the object that says it is one.
        for (const auto& entry : objects_) {
            if (entry.second.member("Type").isName("Catalog")) catalog_ = entry.second;
        }
    }
    std::set<int> seen;
    collectPages(catalog_.member("Pages"), PdfObject(), 792, 0, seen);
    if (pages_.empty()) throw std::runtime_error("The PDF has no pages.");
}

void PdfDocument::scan() {
    std::map<int, size_t> definedAt;
    size_t at = 0;
    while ((at = data_.find("obj", at)) != std::string::npos) {
        size_t keywordEnd = at + 3;
        size_t p = at;
        // `12 0 obj`, standing alone: digits, space, digits, space.
        bool alone = (keywordEnd >= data_.size() || isSpace(data_[keywordEnd]) || data_[keywordEnd] == '<' || data_[keywordEnd] == '[')
            && p > 0 && isSpace(data_[p - 1]);
        while (alone && p > 0 && isSpace(data_[p - 1])) --p;
        size_t generationEnd = p;
        while (alone && p > 0 && isDigit(data_[p - 1])) --p;
        alone = alone && p < generationEnd && p > 0 && isSpace(data_[p - 1]);
        while (alone && p > 0 && isSpace(data_[p - 1])) --p;
        size_t numberEnd = p;
        while (alone && p > 0 && isDigit(data_[p - 1])) --p;
        alone = alone && p < numberEnd && (p == 0 || !isDigit(data_[p - 1]));
        if (!alone) {
            at = keywordEnd;
            continue;
        }
        int number = std::atoi(data_.substr(p, numberEnd - p).c_str());

        PdfLexer lexer(data_, keywordEnd);
        PdfObject object = lexer.next();
        size_t after = lexer.position();
        PdfObject word = lexer.next();
        if (object.is(PdfObject::Type::Dictionary) && word.is(PdfObject::Type::Operator) && word.text == "stream") {
            size_t start = lexer.position();
            if (start < data_.size() && data_[start] == '\r') ++start;
            if (start < data_.size() && data_[start] == '\n') ++start;
            size_t end = std::string::npos;
            // The length, when it is written out and lands on `endstream`;
            // otherwise wherever `endstream` is.
            const PdfObject& length = object.member("Length");
            if (length.is(PdfObject::Type::Number) && length.number >= 0 && start + length.number <= data_.size()) {
                size_t candidate = start + static_cast<size_t>(length.number);
                size_t q = candidate;
                while (q < data_.size() && isSpace(data_[q])) ++q;
                if (data_.compare(q, 9, "endstream") == 0) end = candidate;
            }
            if (end == std::string::npos) {
                end = std::min(data_.find("endstream", start), data_.size());
                while (end > start && (data_[end - 1] == '\n' || data_[end - 1] == '\r')) --end;
            }
            object.type = PdfObject::Type::Stream;
            object.streamStart = start;
            object.streamLength = end - start;
            after = end;
        }
        objects_[number] = std::move(object);
        definedAt[number] = at;
        at = std::max(after, keywordEnd);
    }
    readObjectStreams(definedAt);
    readTrailer(definedAt);
}

void PdfDocument::readObjectStreams(std::map<int, size_t>& definedAt) {
    std::vector<std::pair<int, size_t>> streams;
    for (const auto& entry : objects_) {
        if (entry.second.is(PdfObject::Type::Stream) && entry.second.member("Type").isName("ObjStm")) {
            streams.emplace_back(entry.first, definedAt[entry.first]);
        }
    }
    for (const auto& stream : streams) {
        const PdfObject& holder = objects_[stream.first];
        std::string bytes = contents(holder);
        int count = get(holder, "N").integer();
        int first = get(holder, "First").integer();
        PdfLexer header(bytes);
        for (int i = 0; i < count && !header.atEnd(); ++i) {
            int number = header.next().integer();
            int offset = header.next().integer();
            if (first + offset < 0 || static_cast<size_t>(first + offset) >= bytes.size()) continue;
            // An object written again later in the file, by an update, wins.
            auto known = definedAt.find(number);
            if (known != definedAt.end() && known->second > stream.second) continue;
            PdfLexer body(bytes, static_cast<size_t>(first + offset));
            objects_[number] = body.next();
            definedAt[number] = stream.second;
        }
    }
}

void PdfDocument::readTrailer(const std::map<int, size_t>& definedAt) {
    // Every trailer and cross-reference stream, oldest first, so a later
    // update's entries override an earlier's.
    std::vector<std::pair<size_t, PdfObject>> trailers;
    for (size_t at = data_.find("trailer"); at != std::string::npos; at = data_.find("trailer", at + 7)) {
        PdfLexer lexer(data_, at + 7);
        PdfObject dictionary = lexer.next();
        if (dictionary.is(PdfObject::Type::Dictionary)) trailers.emplace_back(at, std::move(dictionary));
    }
    for (const auto& entry : objects_) {
        if (entry.second.is(PdfObject::Type::Stream) && entry.second.member("Type").isName("XRef")) {
            auto found = definedAt.find(entry.first);
            trailers.emplace_back(found == definedAt.end() ? 0 : found->second, entry.second);
        }
    }
    std::stable_sort(trailers.begin(), trailers.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    trailer_.type = PdfObject::Type::Dictionary;
    for (const auto& trailer : trailers) {
        for (const auto& member : trailer.second.members) {
            if (member.first != "Root" && member.first != "Info" && member.first != "Encrypt") continue;
            auto existing = std::find_if(trailer_.members.begin(), trailer_.members.end(),
                                         [&](const auto& candidate) { return candidate.first == member.first; });
            if (existing != trailer_.members.end()) existing->second = member.second;
            else trailer_.members.push_back(member);
        }
    }
}

const PdfObject& PdfDocument::resolve(const PdfObject& value) const {
    static const PdfObject null;
    const PdfObject* current = &value;
    for (int hops = 0; current->is(PdfObject::Type::Reference); ++hops) {
        auto found = objects_.find(current->integer());
        if (found == objects_.end() || hops > 32) return null;
        current = &found->second;
    }
    return *current;
}

const PdfObject& PdfDocument::get(const PdfObject& dictionary, const std::string& key) const {
    return resolve(resolve(dictionary).member(key));
}

std::string PdfDocument::contents(const PdfObject& stream) const {
    if (!stream.is(PdfObject::Type::Stream) || stream.streamStart >= data_.size()) return "";
    std::string bytes = data_.substr(stream.streamStart, stream.streamLength);
    const PdfObject& filter = get(stream, "Filter");
    std::vector<PdfObject> filters = filter.is(PdfObject::Type::Array) ? filter.items : std::vector<PdfObject>{filter};
    for (const auto& name : filters) {
        const PdfObject& resolved = resolve(name);
        if (!resolved.is(PdfObject::Type::Name)) continue;
        auto decoded = PdfFilters::decode(resolved.text, bytes);
        if (!decoded) return "";
        bytes = std::move(*decoded);
    }
    return bytes;
}

void PdfDocument::collectPages(const PdfObject& node, const PdfObject& resources, double top, int depth, std::set<int>& seen) {
    if (depth > 32) return;
    if (node.is(PdfObject::Type::Reference) && !seen.insert(node.integer()).second) return;
    const PdfObject& page = resolve(node);
    const PdfObject& own = get(page, "Resources");
    const PdfObject& inherited = own.is(PdfObject::Type::Dictionary) ? own : resources;
    const PdfObject& box = get(page, "MediaBox");
    if (box.is(PdfObject::Type::Array) && box.items.size() == 4) {
        top = std::max(resolve(box.items[1]).number, resolve(box.items[3]).number);
    }
    const PdfObject& kids = get(page, "Kids");
    if (kids.is(PdfObject::Type::Array)) {
        for (const auto& kid : kids.items) collectPages(kid, inherited, top, depth + 1, seen);
        return;
    }
    if (!page.is(PdfObject::Type::Dictionary)) return;
    int number = node.is(PdfObject::Type::Reference) ? node.integer() : -1;
    if (number >= 0) pageIndex_[number] = static_cast<int>(pages_.size());
    pages_.push_back({number, page, inherited, top});
}

std::vector<PdfBookmark> PdfDocument::outline() const {
    std::map<std::string, PdfObject> named;
    const PdfObject& dests = get(catalog_, "Dests");
    for (const auto& member : dests.members) named[member.first] = member.second;
    namedDestinations(get(get(catalog_, "Names"), "Dests"), named, 0);

    auto siblings = [this](const PdfObject& first) {
        std::vector<const PdfObject*> items;
        std::set<int> seen;
        for (const PdfObject* link = &first; !resolve(*link).isNull() && items.size() < 10000;) {
            if (link->is(PdfObject::Type::Reference) && !seen.insert(link->integer()).second) break;
            items.push_back(&resolve(*link));
            link = &resolve(*link).member("Next");
        }
        return items;
    };
    auto items = siblings(get(catalog_, "Outlines").member("First"));
    if (items.size() == 1 && !items[0]->member("First").isNull()) items = siblings(items[0]->member("First"));

    std::vector<PdfBookmark> bookmarks;
    for (const PdfObject* item : items) {
        int page = destinationPage(*item, named);
        if (page >= 0) bookmarks.push_back({PdfEncodings::text(get(*item, "Title").text), page});
    }
    return bookmarks;
}

void PdfDocument::namedDestinations(const PdfObject& node, std::map<std::string, PdfObject>& named, int depth) const {
    if (depth > 16) return;
    const PdfObject& names = get(node, "Names");
    for (size_t i = 0; i + 1 < names.items.size(); i += 2) named[resolve(names.items[i]).text] = names.items[i + 1];
    for (const auto& kid : get(node, "Kids").items) namedDestinations(kid, named, depth + 1);
}

int PdfDocument::destinationPage(const PdfObject& item, const std::map<std::string, PdfObject>& named) const {
    const PdfObject* destination = &get(item, "Dest");
    if (destination->isNull()) {
        const PdfObject& action = get(item, "A");
        if (get(action, "S").isName("GoTo")) destination = &get(action, "D");
    }
    if (destination->is(PdfObject::Type::Name) || destination->is(PdfObject::Type::String)) {
        auto found = named.find(destination->text);
        if (found == named.end()) return -1;
        destination = &resolve(found->second);
    }
    if (destination->is(PdfObject::Type::Dictionary)) destination = &get(*destination, "D");
    if (!destination->is(PdfObject::Type::Array) || destination->items.empty()) return -1;
    const PdfObject& target = destination->items[0];
    if (target.is(PdfObject::Type::Number)) return target.integer() < static_cast<int>(pages_.size()) ? target.integer() : -1;
    auto found = pageIndex_.find(target.integer());
    return target.is(PdfObject::Type::Reference) && found != pageIndex_.end() ? found->second : -1;
}

std::string PdfDocument::info(const std::string& key) const {
    const PdfObject& value = get(get(trailer_, "Info"), key);
    return value.is(PdfObject::Type::String) ? PdfEncodings::text(value.text) : "";
}

std::string PdfDocument::language() const {
    const PdfObject& value = get(catalog_, "Lang");
    return value.is(PdfObject::Type::String) ? PdfEncodings::text(value.text) : "";
}
