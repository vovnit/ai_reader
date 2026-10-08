#include "LibraryFeature.hpp"

#include "../../Services/EpubLoader.hpp"
#include "../../Services/LibrarySync.hpp"
#include "../../Services/Paths.hpp"
#include "../../Services/PdfImporter.hpp"
#include "../../Support/Files.hpp"
#include "../../Support/Text.hpp"
#include "../../Support/Async.hpp"
#include "../Common/SyncRunner.hpp"

#include <cstdio>

namespace {

/// Runs a step of the exchange on a worker. What came of it is written down
/// even if the screen has closed by then; `done` runs only while it is open.
void exchangeFiles(Env& env, std::function<LibrarySync::Outcome()> work, const std::shared_ptr<bool>& alive,
                   std::function<void(const LibrarySync::Outcome&)> done) {
    static auto forever = std::make_shared<bool>(true);
    Env* environment = &env;
    Async::run<LibrarySync::Outcome>(std::move(work),
        [environment, screen = std::weak_ptr<bool>(alive), done](LibrarySync::Outcome outcome) {
            LibrarySync::record(*environment, outcome);
            if (screen.lock()) done(outcome);
        },
        forever);
}

}  // namespace

LibraryFeature::LibraryFeature(Env& env) : env_(env) {}

std::vector<Book> LibraryFeature::booksIn(long long groupId) const {
    std::vector<Book> books;
    for (const auto& book : books_) {
        if (book.groupId == groupId) books.push_back(book);
    }
    return books;
}

bool LibraryFeature::isInCloud(const Book& book) const {
    return !book.remoteName.empty() && remote_.count(book.remoteName);
}

void LibraryFeature::reload() {
    books_ = env_.library.all();
    groups_ = env_.groups.all();
    remote_.clear();
    cloud_.clear();
    if (env_.settings.sync().isConfigured()) remote_ = env_.library.remoteNames();
    std::set<std::string> here;
    for (const auto& book : books_) here.insert(book.remoteName);
    for (const auto& name : remote_) {
        if (!here.count(name)) cloud_.push_back(name);
    }
    if (onChange) onChange();
}

void LibraryFeature::refresh() {
    env_.library.refresh(Paths::bookFolders());
    reload();
}

void LibraryFeature::remove(const Book& book) {
    env_.library.remove(book.id);
    Files::remove(book.path);
    reload();
}

void LibraryFeature::removeEverywhere(const Book& book) {
    SyncSettings settings = env_.settings.sync();
    std::string name = book.remoteName;
    exchangeFiles(env_, [settings, name] { return LibrarySync::remove(settings, name); }, alive_,
        [this, book](const LibrarySync::Outcome& outcome) {
            // The file first: if it cannot go, the book stays as it was.
            if (outcome.error.empty()) remove(book);
            else if (onFailure) onFailure("Couldn’t delete “" + book.title + "”", outcome.error);
        });
}

void LibraryFeature::removeRemote(const std::string& name) {
    SyncSettings settings = env_.settings.sync();
    exchangeFiles(env_, [settings, name] { return LibrarySync::remove(settings, name); }, alive_,
        [this, name](const LibrarySync::Outcome& outcome) {
            if (!outcome.error.empty() && onFailure) onFailure("Couldn’t delete “" + Files::stem(name) + "”", outcome.error);
            reload();
        });
}

void LibraryFeature::download(const std::string& name) {
    SyncSettings settings = env_.settings.sync();
    if (!downloading_.insert(name).second) return;
    reload();
    std::vector<Book> books = env_.library.all();
    exchangeFiles(env_, [settings, books, name] { return LibrarySync::fetch(settings, books, name); }, alive_,
        [this, name](const LibrarySync::Outcome& outcome) {
            downloading_.erase(name);
            if (!outcome.error.empty() && onFailure) onFailure("Couldn’t download “" + Files::stem(name) + "”", outcome.error);
            reload();
            // Where another device is in it, and its group, come with the
            // document.
            if (outcome.error.empty()) sync();
        });
}

std::string LibraryFeature::add(const std::string& path) {
    std::string error;
    // A PDF becomes an EPUB in the books folder, and is shelved as one.
    if (Files::extension(path) == "pdf") {
        if (PdfImporter::convert(path, Paths::books(), &error).empty()) return error;
        refresh();
        return "";
    }
    if (!EpubLoader::metadata(path, &error)) return error.empty() ? "Not an EPUB this app can read." : error;
    // A file already in a scanned folder is picked up where it is.
    std::string folder = Files::directoryName(path);
    bool scanned = false;
    for (const auto& known : Paths::bookFolders()) scanned = scanned || known == folder;
    if (!scanned) {
        std::string destination = Files::join(Paths::books(), Files::baseName(path));
        if (!Files::exists(destination) && !Files::copy(path, destination)) {
            return "Could not copy the book into " + Paths::books() + ".";
        }
    }
    refresh();
    return "";
}

void LibraryFeature::assign(const Book& book, long long groupId) {
    env_.library.assignGroup(book.id, groupId);
    reload();
}

long long LibraryFeature::groupNamed(const std::string& name) {
    std::string trimmed = Text::trim(name);
    return trimmed.empty() ? 0 : env_.groups.named(trimmed);
}

void LibraryFeature::dissolve(const BookGroup& group) {
    env_.groups.remove(group.id);
    reload();
}

void LibraryFeature::sync() {
    if (isSyncing_) return;
    isSyncing_ = true;
    SyncRunner::run(env_, alive_, [this](const std::string& message, bool failed) {
        isSyncing_ = false;
        if (failed) std::fprintf(stderr, "aireader: sync: %s\n", message.c_str());
        reload();
    });
}
