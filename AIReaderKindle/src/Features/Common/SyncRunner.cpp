#include "SyncRunner.hpp"

#include "../../Services/LibrarySync.hpp"
#include "../../Services/Sync.hpp"
#include "../../Support/Async.hpp"

#include <cstdio>

namespace SyncRunner {

void run(Env& env, std::shared_ptr<bool> alive, std::function<void(const std::string&, bool)> done) {
    SyncSettings settings = env.settings.sync();
    if (!settings.isConfigured()) return;

    struct Fetched {
        LibrarySync::Outcome books;
        Sync::Fetched records;
        std::string error;
    };
    Env* environment = &env;
    std::vector<Book> books = env.library.all();
    Sync::Local local = Sync::gather(env, settings);
    static auto forever = std::make_shared<bool>(true);
    Async::run<Fetched>(
        [settings, books, local] {
            Fetched fetched;
            fetched.books = LibrarySync::exchange(settings, books);
            fetched.error = fetched.books.error;
            if (!fetched.error.empty()) return fetched;
            try {
                fetched.records = Sync::fetch(settings, local);
            } catch (const std::exception& failure) {
                fetched.error = failure.what();
            }
            return fetched;
        },
        [environment, settings, screen = std::weak_ptr<bool>(alive), done](Fetched fetched) {
            // Written down even when the screen has closed, or the same
            // books would be sent again next time.
            LibrarySync::record(*environment, fetched.books);
            auto alive = screen.lock();
            if (!alive) return;
            if (!fetched.error.empty()) {
                done(fetched.error, true);
                return;
            }
            Sync::Round round = Sync::reconcile(*environment, fetched.records);
            Sync::Report report{fetched.books.sent, round.applied, !round.outgoing.empty()};
            Async::run<Sync::Sent>(
                [settings, round] { return Sync::send(settings, round); },
                // Also written down whatever the screen does, or the same
                // files would be read and sent again.
                [environment, settings, round, report, screen, done](Sync::Sent sent) {
                    Sync::record(*environment, settings, round, sent);
                    if (!screen.lock()) return;
                    done(sent.error.empty() ? report.summary() : sent.error, !sent.error.empty());
                },
                forever);
        },
        forever);
}

}  // namespace SyncRunner
