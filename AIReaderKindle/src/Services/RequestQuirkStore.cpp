#include "RequestQuirkStore.hpp"

#include "../Support/Files.hpp"
#include "../Support/Text.hpp"

#include <vector>

RequestQuirkStore& RequestQuirkStore::shared() {
    static RequestQuirkStore store;
    return store;
}

void RequestQuirkStore::keepIn(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
    known_.clear();
    auto contents = Files::read(path);
    if (!contents) return;
    // A line a model: its endpoint and name, a tab, and what it needed.
    for (const auto& line : Text::split(*contents, '\n')) {
        auto tab = line.rfind('\t');
        if (tab == std::string::npos) continue;
        for (const auto& name : Text::split(line.substr(tab + 1), ',')) {
            if (auto quirk = requestQuirkCalled(name)) known_[line.substr(0, tab)].insert(*quirk);
        }
    }
}

std::set<RequestQuirk> RequestQuirkStore::quirks(const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto known = known_.find(model);
    return known == known_.end() ? std::set<RequestQuirk>() : known->second;
}

void RequestQuirkStore::learn(RequestQuirk quirk, const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (known_[model].insert(quirk).second) save();
}

void RequestQuirkStore::forget(const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (known_.erase(model)) save();
}

void RequestQuirkStore::save() {
    if (path_.empty()) return;
    std::string contents;
    for (const auto& [model, quirks] : known_) {
        std::vector<std::string> names;
        for (auto quirk : quirks) names.push_back(requestQuirkName(quirk));
        contents += model + "\t" + Text::join(names, ",") + "\n";
    }
    Files::write(path_, contents);
}
