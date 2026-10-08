#include "Sync.hpp"

#include "../Support/Text.hpp"
#include "WebDav.hpp"

#include <map>
#include <set>

namespace Sync {

Local gather(Env& env, const SyncSettings& settings) {
    return {SyncStore::exportAll(env), SyncStore::known(env, settings.partsUrl())};
}

Fetched fetch(const SyncSettings& settings, const Local& local) {
    Fetched fetched;
    for (const auto& entry : WebDav::list(settings.partsUrl(), settings)) {
        fetched.listed.push_back({entry.name, entry.version, ""});
    }

    std::string oldKnown;
    for (const auto& file : local.known) {
        if (file.name == SyncParts::oldFile) oldKnown = file.version;
    }
    for (const auto& entry : WebDav::list(settings.oldFileUrl(), settings)) {
        if (entry.name != SyncParts::oldFile || (!entry.version.empty() && entry.version == oldKnown)) continue;
        auto contents = WebDav::download(settings.oldFileUrl(), settings);
        // One that cannot be read is passed over rather than stopping every
        // sync from now on.
        auto document = contents ? SyncDocument::parse(*contents) : std::nullopt;
        if (document) fetched.old = *document;
        fetched.oldVersion = entry.version;
    }

    std::set<std::string> onServer;
    for (const auto& file : fetched.listed) onServer.insert(file.name);
    for (const auto& name : SyncParts::due(fetched.listed, local.known, local.records, fetched.old)) {
        SyncParts::Part part{name, {}};
        auto contents = onServer.count(name) ? WebDav::download(settings.partsUrl() + name, settings) : std::nullopt;
        if (contents) {
            auto document = SyncDocument::parse(*contents);
            if (!document) throw WebDav::Error("The sync file " + name + " on the server could not be read.");
            part.records = *document;
        }
        fetched.remote.push_back(part);
    }
    return fetched;
}

Round reconcile(Env& env, const Fetched& fetched) {
    Round round;
    round.oldVersion = fetched.oldVersion;
    auto merged = SyncParts::merge(fetched.remote, SyncStore::exportAll(env), fetched.old);
    round.applied = SyncStore::apply(env, SyncParts::join(merged));

    std::map<std::string, std::string> versions;
    for (const auto& file : fetched.listed) versions[file.name] = file.version;
    std::map<std::string, std::string> digests;
    for (const auto& file : SyncParts::fingerprints(SyncStore::exportAll(env))) digests[file.name] = file.digest;
    for (size_t i = 0; i < merged.size(); ++i) {
        const auto& part = merged[i];
        if (part.records != fetched.remote[i].records.sorted()) round.outgoing.push_back(part);
        round.settled.push_back({part.name, versions[part.name], digests[part.name]});
    }
    return round;
}

Sent send(const SyncSettings& settings, const Round& round) {
    Sent sent;
    try {
        for (const auto& part : round.outgoing) {
            std::string version = WebDav::upload(settings.partsUrl() + part.name, part.records.dump(), settings);
            sent.files.push_back({part.name, version, ""});
        }
    } catch (const std::exception& failure) {
        sent.error = failure.what();
    }
    return sent;
}

void record(Env& env, const SyncSettings& settings, const Round& round, const Sent& sent) {
    std::map<std::string, std::string> versions;
    for (const auto& file : sent.files) versions[file.name] = file.version;
    std::set<std::string> unsent;
    for (const auto& part : round.outgoing) {
        if (!versions.count(part.name)) unsent.insert(part.name);
    }
    std::vector<SyncParts::File> settled;
    for (auto file : round.settled) {
        if (unsent.count(file.name)) continue;
        auto version = versions.find(file.name);
        if (version != versions.end()) file.version = version->second;
        settled.push_back(file);
    }
    // The old file counts as read once everything it brought is on the
    // server.
    if (!round.oldVersion.empty() && unsent.empty()) settled.push_back({SyncParts::oldFile, round.oldVersion, ""});
    std::string folder = settings.partsUrl();
    SyncStore::remember(env, folder, SyncParts::remember(SyncStore::known(env, folder), settled));
}

std::string Report::summary() const {
    std::vector<std::string> parts;
    if (applied.lookups > 0) parts.push_back(std::to_string(applied.lookups) + (applied.lookups == 1 ? " word" : " words"));
    if (applied.books > 0) parts.push_back(std::to_string(applied.books) + (applied.books == 1 ? " book update" : " book updates"));
    std::string received = parts.empty() ? "Nothing new here" : "Received " + Text::join(parts, ", ");
    if (!uploaded && sent == 0) return received + "; the server was up to date.";
    std::string books = sent > 0 ? "; sent " + std::to_string(sent) + (sent == 1 ? " book" : " books") : "";
    return received + books + (uploaded ? "; sent changes." : ".");
}

}  // namespace Sync
