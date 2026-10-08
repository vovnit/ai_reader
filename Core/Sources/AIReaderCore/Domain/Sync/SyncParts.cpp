#include "SyncParts.hpp"

#include <cstdint>
#include <map>
#include <set>

const char* const SyncParts::folder = "aireader-sync";
const char* const SyncParts::oldFile = "aireader-sync.json";

namespace {

/// FNV-1a, 64 bits, over the bytes: small, and the same in JavaScript.
uint64_t fnv(const std::string& bytes) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hex(uint64_t value, int digits) {
    static const char* const alphabet = "0123456789abcdef";
    std::string text(digits, '0');
    for (int i = digits - 1; i >= 0; --i, value >>= 4) text[i] = alphabet[value & 0xF];
    return text;
}

bool isHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

std::map<std::string, SyncDocument> byName(const SyncDocument& document) {
    std::map<std::string, SyncDocument> parts;
    for (const auto& record : document.books) parts[SyncParts::nameOf(record.key)].books.push_back(record);
    for (const auto& record : document.lookups) parts[SyncParts::nameOf(record.key())].lookups.push_back(record);
    return parts;
}

}  // namespace

std::string SyncParts::nameOf(const std::string& key) {
    // The top byte, since FNV mixes its low bits least.
    return hex(fnv(key) >> 56, 2) + ".json";
}

bool SyncParts::isPart(const std::string& name) {
    return name.size() == 7 && isHex(name[0]) && isHex(name[1]) && name.compare(2, 5, ".json") == 0;
}

SyncParts::Parts SyncParts::split(const SyncDocument& document) {
    Parts parts;
    for (const auto& entry : byName(document)) parts.push_back({entry.first, entry.second.sorted()});
    return parts;
}

SyncDocument SyncParts::join(const Parts& parts) {
    SyncDocument joined;
    for (const auto& part : parts) {
        joined.books.insert(joined.books.end(), part.records.books.begin(), part.records.books.end());
        joined.lookups.insert(joined.lookups.end(), part.records.lookups.begin(), part.records.lookups.end());
    }
    return joined.sorted();
}

std::string SyncParts::digest(const SyncDocument& records) {
    if (records.books.empty() && records.lookups.empty()) return "";
    return hex(fnv(records.dump()), 16);
}

SyncParts::Files SyncParts::fingerprints(const SyncDocument& local) {
    Files files;
    for (const auto& entry : byName(local)) files.push_back({entry.first, "", digest(entry.second)});
    return files;
}

SyncParts::Names SyncParts::due(const Files& listed, const Files& known, const SyncDocument& local,
                                const SyncDocument& old) {
    std::map<std::string, std::string> server;
    for (const auto& file : listed) {
        if (isPart(file.name)) server[file.name] = file.version;
    }
    std::map<std::string, File> knew;
    for (const auto& file : known) {
        if (isPart(file.name)) knew[file.name] = file;
    }
    std::map<std::string, std::string> here;
    for (const auto& file : fingerprints(local)) here[file.name] = file.digest;

    std::set<std::string> due;
    for (const auto& [name, version] : server) {
        auto seen = knew.find(name);
        // A server that gives no version leaves every file to be read.
        if (seen == knew.end() || version.empty() || version != seen->second.version) due.insert(name);
    }
    for (const auto& [name, file] : knew) {
        auto digest = here.find(name);
        if (!server.count(name) || (digest == here.end() ? "" : digest->second) != file.digest) due.insert(name);
    }
    for (const auto& entry : here) {
        if (!knew.count(entry.first)) due.insert(entry.first);
    }
    for (const auto& entry : byName(old)) due.insert(entry.first);
    return Names(due.begin(), due.end());
}

SyncParts::Parts SyncParts::merge(const Parts& remote, const SyncDocument& local, const SyncDocument& old) {
    auto mine = byName(local);
    auto older = byName(old);
    Parts merged;
    for (const auto& part : remote) {
        // The server's side last, so where two records tie it keeps its own
        // and is not written again for nothing.
        SyncDocument here = SyncDocument::merge(mine[part.name], older[part.name]);
        merged.push_back({part.name, SyncDocument::merge(here, part.records)});
    }
    return merged;
}

SyncParts::Files SyncParts::remember(const Files& known, const Files& settled) {
    std::map<std::string, File> files;
    for (const auto& file : known) files[file.name] = file;
    for (const auto& file : settled) {
        if (file.version.empty() && file.digest.empty()) {
            files.erase(file.name);
        } else {
            files[file.name] = file;
        }
    }
    Files remembered;
    for (const auto& entry : files) remembered.push_back(entry.second);
    return remembered;
}
