#include "LibrarySync.hpp"

#include "../Domain/Books/BookKey.hpp"
#include "../Domain/Books/RemoteBookName.hpp"
#include "../Support/Files.hpp"
#include "../Support/Text.hpp"
#include "EpubLoader.hpp"
#include "Paths.hpp"
#include "WebDav.hpp"

#include <map>
#include <stdexcept>

namespace LibrarySync {

Outcome exchange(const SyncSettings& settings, const std::vector<Book>& books) {
    Outcome outcome;
    std::string folder = settings.booksUrl();
    try {
        std::vector<std::string> remote;
        for (const auto& file : WebDav::list(folder, settings)) {
            if (RemoteBookName::isBook(file.name)) remote.push_back(file.name);
        }
        outcome.listed = remote;
        // Compared without case, since some servers ignore it.
        std::map<std::string, std::string> byLowercase;
        for (const auto& name : remote) byLowercase.emplace(Text::lower(name), name);

        for (const auto& book : books) {
            if (!book.remoteName.empty()) continue;
            auto same = byLowercase.find(Text::lower(RemoteBookName::make(book.title, book.author, {})));
            if (same != byLowercase.end()) {
                outcome.named.push_back({book.id, same->second});
                continue;
            }
            auto contents = Files::read(book.path);
            if (!contents) continue;
            std::string name = RemoteBookName::make(book.title, book.author, *outcome.listed);
            WebDav::upload(folder + WebDav::escape(name), *contents, settings, "application/epub+zip");
            outcome.listed->push_back(name);
            outcome.named.push_back({book.id, name});
            outcome.sent += 1;
        }
    } catch (const std::exception& failure) {
        outcome.error = failure.what();
    }
    return outcome;
}

Outcome fetch(const SyncSettings& settings, const std::vector<Book>& books, const std::string& name) {
    Outcome outcome;
    try {
        auto contents = WebDav::download(settings.booksUrl() + WebDav::escape(name), settings);
        if (!contents) {
            outcome.gone.push_back(name);
            throw std::runtime_error("It is no longer in the sync folder.");
        }
        std::string file = RemoteBookName::make(Files::stem(name), "", Files::list(Paths::books()));
        std::string path = Files::join(Paths::books(), file);
        if (!Files::write(path, *contents)) throw std::runtime_error("Could not save it to " + Paths::books() + ".");
        auto metadata = EpubLoader::metadata(path);
        if (!metadata) {
            Files::remove(path);
            throw std::runtime_error("Not an EPUB this app can read.");
        }
        std::string key = BookKey::make(metadata->title, metadata->author);
        for (const auto& book : books) {
            if (BookKey::make(book.title, book.author) != key) continue;
            Files::remove(path);
            if (!book.remoteName.empty()) throw std::runtime_error("“" + book.title + "” is already on the shelf.");
            outcome.named.push_back({book.id, name});
            return outcome;
        }
        outcome.received.push_back({name, path});
    } catch (const std::exception& failure) {
        outcome.error = failure.what();
    }
    return outcome;
}

Outcome remove(const SyncSettings& settings, const std::string& name) {
    Outcome outcome;
    try {
        WebDav::remove(settings.booksUrl() + WebDav::escape(name), settings);
        outcome.gone.push_back(name);
    } catch (const std::exception& failure) {
        outcome.error = failure.what();
    }
    return outcome;
}

void record(Env& env, const Outcome& outcome) {
    if (outcome.listed) env.library.setRemoteNames(*outcome.listed);
    for (const auto& name : outcome.gone) env.library.forgetRemote(name);
    if (!outcome.received.empty()) env.library.refresh(Paths::bookFolders());
    for (const auto& book : env.library.all()) {
        for (const auto& [name, path] : outcome.received) {
            if (book.path == path) env.library.setRemoteName(book.id, name);
        }
    }
    for (const auto& [id, name] : outcome.named) env.library.setRemoteName(id, name);
}

}  // namespace LibrarySync
