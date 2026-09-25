#include "LibrarySync.hpp"

#include "../Domain/Books/BookKey.hpp"
#include "../Domain/Books/RemoteBookName.hpp"
#include "../Support/Files.hpp"
#include "EpubLoader.hpp"
#include "Paths.hpp"
#include "WebDav.hpp"

#include <map>

namespace LibrarySync {

Local gather(Env& env) {
    return {env.library.all(), env.library.remoteNamesMet()};
}

Outcome exchange(const SyncSettings& settings, const Local& local) {
    Outcome outcome;
    std::string folder = settings.booksUrl();
    std::map<std::string, long long> keys;
    std::set<long long> named;
    for (const auto& book : local.books) {
        keys[BookKey::make(book.title, book.author)] = book.id;
        if (!book.remoteName.empty()) named.insert(book.id);
    }

    try {
        std::vector<std::string> remote;
        for (const auto& name : WebDav::list(folder, settings)) {
            if (RemoteBookName::isBook(name)) remote.push_back(name);
        }

        // Fetched before anything is sent, so a book this device already
        // has is recognised and not sent a second time.
        for (const auto& name : remote) {
            if (local.met.count(name)) continue;
            auto contents = WebDav::download(folder + WebDav::escape(name), settings);
            // A file that is not a readable EPUB is met all the same, so it
            // is not fetched again on every sync.
            outcome.met.push_back(name);
            if (!contents) continue;
            std::string file = RemoteBookName::make(Files::stem(name), "", Files::list(Paths::books()));
            std::string path = Files::join(Paths::books(), file);
            if (!Files::write(path, *contents)) throw WebDav::Error("Could not save " + name + " to " + Paths::books() + ".");
            auto metadata = EpubLoader::metadata(path);
            if (!metadata) {
                Files::remove(path);
                continue;
            }
            std::string key = BookKey::make(metadata->title, metadata->author);
            auto same = keys.find(key);
            if (same == keys.end()) {
                keys[key] = 0;
                outcome.received.push_back({name, path});
                continue;
            }
            Files::remove(path);
            if (same->second && !named.count(same->second)) {
                named.insert(same->second);
                outcome.named.push_back({same->second, name});
            }
        }

        std::vector<std::string> taken = remote;
        for (const auto& book : local.books) {
            if (named.count(book.id)) continue;
            auto contents = Files::read(book.path);
            if (!contents) continue;
            std::string name = RemoteBookName::make(book.title, book.author, taken);
            WebDav::upload(folder + WebDav::escape(name), *contents, settings, "application/epub+zip");
            taken.push_back(name);
            outcome.named.push_back({book.id, name});
            outcome.met.push_back(name);
            outcome.sent += 1;
        }
    } catch (const std::exception& failure) {
        outcome.error = failure.what();
    }
    return outcome;
}

void record(Env& env, const Outcome& outcome) {
    for (const auto& name : outcome.met) env.library.meetRemote(name);
    if (!outcome.received.empty()) env.library.refresh(Paths::bookFolders());
    for (const auto& book : env.library.all()) {
        for (const auto& [name, path] : outcome.received) {
            if (book.path == path) env.library.setRemoteName(book.id, name);
        }
    }
    for (const auto& [id, name] : outcome.named) env.library.setRemoteName(id, name);
}

}  // namespace LibrarySync
