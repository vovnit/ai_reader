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
        SyncDocument remote;
        std::string error;
    };
    Env* environment = &env;
    LibrarySync::Local local = LibrarySync::gather(env);
    static auto forever = std::make_shared<bool>(true);
    Async::run<Fetched>(
        [settings, local] {
            Fetched fetched;
            // Books first, so the places and groups of any that arrive are
            // applied in this same round.
            fetched.books = LibrarySync::exchange(settings, local);
            fetched.error = fetched.books.error;
            if (!fetched.error.empty()) return fetched;
            try {
                fetched.remote = Sync::fetch(settings);
            } catch (const std::exception& failure) {
                fetched.error = failure.what();
            }
            return fetched;
        },
        [environment, settings, screen = std::weak_ptr<bool>(alive), done](Fetched fetched) {
            // Written down even when the screen has closed, or the same
            // books would be fetched and sent again next time.
            LibrarySync::record(*environment, fetched.books);
            auto alive = screen.lock();
            if (!alive) return;
            if (!fetched.error.empty()) {
                done(fetched.error, true);
                return;
            }
            SyncDocument merged;
            Sync::Report report = Sync::reconcile(*environment, fetched.remote, merged);
            report.received = static_cast<int>(fetched.books.received.size());
            report.sent = fetched.books.sent;
            if (!report.uploaded) {
                done(report.summary(), false);
                return;
            }
            Async::run<std::string>(
                [settings, merged] {
                    try {
                        Sync::store(settings, merged);
                        return std::string();
                    } catch (const std::exception& failure) {
                        return std::string(failure.what());
                    }
                },
                [report, done](std::string error) {
                    done(error.empty() ? report.summary() : error, !error.empty());
                },
                alive);
        },
        forever);
}

}  // namespace SyncRunner
