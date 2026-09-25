#include "DictionariesFeature.hpp"

#include "../../Support/Text.hpp"

DictionariesFeature::DictionariesFeature(Env& env) : env_(env) {
    packs_ = env_.packs.all();
}

void DictionariesFeature::toggle(const DictionaryPack& pack) {
    env_.packs.setEnabled(pack.id, !pack.isEnabled);
    reload();
}

void DictionariesFeature::remove(const DictionaryPack& pack) {
    env_.packs.remove(pack);
    reload();
}

std::string DictionariesFeature::addFromFolder() {
    std::vector<std::string> errors;
    int added = env_.packs.addFromFolder(errors);
    reload();
    return report(added, errors);
}

std::string DictionariesFeature::addFile(const std::string& path) {
    std::vector<std::string> errors;
    int added = env_.packs.addFile(path, errors);
    reload();
    if (added == 0 && errors.empty()) return "Already in the list.";
    return report(added, errors);
}

std::string DictionariesFeature::report(int added, const std::vector<std::string>& errors) {
    std::string message = added == 1 ? "Added 1 dictionary." : "Added " + std::to_string(added) + " dictionaries.";
    if (!errors.empty()) message += "\n\n" + Text::join(errors, "\n");
    return message;
}

void DictionariesFeature::reload() {
    packs_ = env_.packs.all();
    if (onChange) onChange();
}
