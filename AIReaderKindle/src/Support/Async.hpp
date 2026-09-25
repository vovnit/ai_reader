#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <glib.h>

/// Runs work on a thread and delivers its result on the main loop. The result
/// is dropped if `guard` has expired by then, so a screen that was closed while
/// waiting is never touched.
///
/// The Kindle's GLib is 2.29, so threads go through `g_thread_create` (and
/// `g_thread_init` in `main`); both still exist, deprecated, on a desktop.
namespace Async {

template <class Result>
class Job {
public:
    Job(std::function<Result()> work, std::function<void(Result)> done, std::weak_ptr<void> guard)
        : work_(std::move(work)), done_(std::move(done)), guard_(std::move(guard)) {}

    static gpointer run(gpointer data) {
        auto* job = static_cast<Job*>(data);
        job->result_ = job->work_();
        g_idle_add(&Job::finish, job);
        return nullptr;
    }

private:
    static gboolean finish(gpointer data) {
        auto* job = static_cast<Job*>(data);
        if (job->guard_.lock()) job->done_(std::move(*job->result_));
        delete job;
        return FALSE;  // one-shot
    }

    std::function<Result()> work_;
    std::function<void(Result)> done_;
    std::weak_ptr<void> guard_;
    std::optional<Result> result_;
};

template <class Result>
void run(std::function<Result()> work, std::function<void(Result)> done, std::weak_ptr<void> guard) {
    auto* job = new Job<Result>(std::move(work), std::move(done), std::move(guard));
    g_thread_create(&Job<Result>::run, job, FALSE, nullptr);
}

}  // namespace Async
