#pragma once

#include "../Domain/AI/RequestQuirks.hpp"

#include <map>
#include <mutex>
#include <set>
#include <string>

/// Remembers which adjustments each endpoint and model needed
/// (`RequestQuirk`), so finding them out — a refused request each — is paid
/// once, not again on every start. Kept in a file once `keepIn` names one;
/// until then, for this run only.
class RequestQuirkStore {
public:
    static RequestQuirkStore& shared();

    /// Reads what earlier runs learned from `path`, and writes there from
    /// now on.
    void keepIn(const std::string& path);
    std::set<RequestQuirk> quirks(const std::string& model);
    void learn(RequestQuirk quirk, const std::string& model);
    /// Forgets what a model needed, once it no longer fits: a service can
    /// change what it accepts.
    void forget(const std::string& model);

private:
    void save();

    std::mutex mutex_;
    std::string path_;
    std::map<std::string, std::set<RequestQuirk>> known_;
};
