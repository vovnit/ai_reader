#include "Sync.hpp"

#include "../Support/Text.hpp"
#include "WebDav.hpp"

#include <vector>

namespace Sync {

std::string Report::summary() const {
    std::vector<std::string> parts;
    if (this->received > 0) parts.push_back(std::to_string(this->received) + (this->received == 1 ? " new book" : " new books"));
    if (applied.lookups > 0) parts.push_back(std::to_string(applied.lookups) + (applied.lookups == 1 ? " word" : " words"));
    if (applied.books > 0) parts.push_back(std::to_string(applied.books) + (applied.books == 1 ? " book update" : " book updates"));
    std::string received = parts.empty() ? "Nothing new here" : "Received " + Text::join(parts, ", ");
    if (!uploaded && sent == 0) return received + "; the server was up to date.";
    std::string books = sent > 0 ? "; sent " + std::to_string(sent) + (sent == 1 ? " book" : " books") : "";
    return received + books + (uploaded ? "; sent changes." : ".");
}

SyncDocument fetch(const SyncSettings& settings) {
    auto contents = WebDav::download(settings.fileUrl(), settings);
    if (!contents) return SyncDocument();
    auto document = SyncDocument::parse(*contents);
    if (!document) throw WebDav::Error("The sync file on the server could not be read.");
    return *document;
}

void store(const SyncSettings& settings, const SyncDocument& document) {
    WebDav::upload(settings.fileUrl(), document.dump(), settings);
}

Report reconcile(Env& env, const SyncDocument& remote, SyncDocument& merged) {
    merged = SyncDocument::merge(SyncStore::exportAll(env), remote);
    Report report;
    report.applied = SyncStore::apply(env, merged);
    report.uploaded = merged != remote.sorted();
    return report;
}

}  // namespace Sync
