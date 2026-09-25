#include "DictionaryPacks.hpp"

#include "../Domain/Formats/DictionaryConverter.hpp"
#include "../Support/Files.hpp"
#include "../Support/Text.hpp"
#include "DictionaryDatabase.hpp"
#include "DictionaryPackWriter.hpp"
#include "Migrations.hpp"
#include "Paths.hpp"

#include <algorithm>
#include <set>

std::vector<DictionaryPack> DictionaryPacks::query(const std::string& where) {
    std::vector<DictionaryPack> packs;
    Statement select(database_,
        "SELECT id, name, fileName, targetLanguage, definitionLanguage, isEnabled, addedAt"
        " FROM dictionaryPacks " + where + " ORDER BY addedAt, id");
    while (select.step()) {
        DictionaryPack pack;
        pack.id = select.integer(0);
        pack.name = select.text(1);
        pack.fileName = select.text(2);
        pack.targetLanguage = select.text(3);
        pack.definitionLanguage = select.text(4);
        pack.isEnabled = select.integer(5) != 0;
        pack.addedAt = select.text(6);
        packs.push_back(pack);
    }
    return packs;
}

std::vector<DictionaryPack> DictionaryPacks::all() { return query(""); }
std::vector<DictionaryPack> DictionaryPacks::enabled() { return query("WHERE isEnabled = 1"); }

void DictionaryPacks::setEnabled(long long id, bool isEnabled) {
    Statement update(database_, "UPDATE dictionaryPacks SET isEnabled = ? WHERE id = ?");
    update.bind(1, isEnabled ? 1 : 0).bind(2, id).run();
}

void DictionaryPacks::remove(const DictionaryPack& pack) {
    if (pack.isBundled()) return;
    Statement remove(database_, "DELETE FROM dictionaryPacks WHERE id = ?");
    remove.bind(1, pack.id).run();
    Files::remove(DictionaryDatabase::path(pack));
}

void DictionaryPacks::insert(const DictionaryPack& pack) {
    Statement insert(database_,
        "INSERT INTO dictionaryPacks (name, fileName, targetLanguage, definitionLanguage, isEnabled, addedAt)"
        " VALUES (?, ?, ?, ?, 1, ?)");
    insert.bind(1, pack.name).bind(2, pack.fileName).bind(3, pack.targetLanguage)
        .bind(4, pack.definitionLanguage).bind(5, Migrations::now()).run();
}

/// A pack this app wrote is taken as it is, once its schema checks out.
bool DictionaryPacks::addNative(const std::string& fileName, std::string& error) {
    auto metadata = DictionaryDatabase::metadata(Files::join(Paths::dictionaries(), fileName));
    if (metadata.empty()) {
        error = fileName + " is not a dictionary this app can read.";
        return false;
    }
    std::string version = metadata.count("schema_version") ? metadata["schema_version"] : "?";
    if (version != "2") {
        error = fileName + " uses schema version " + version + "; this app reads version 2.";
        return false;
    }
    DictionaryPack pack;
    pack.name = Files::stem(fileName);
    if (Text::endsWith(pack.name, ".converted")) pack.name = pack.name.substr(0, pack.name.size() - 10);
    pack.fileName = fileName;
    pack.targetLanguage = metadata["target_language"];
    pack.definitionLanguage = metadata["definition_language"];
    insert(pack);
    return true;
}

/// A dictionary in another format becomes a pack named after it, kept
/// beside the source file so it is not converted again.
bool DictionaryPacks::convert(const DictionarySource& source, std::string& error) {
    std::string packName = DictionaryFormats::stem(source.main) + ".converted.sqlite3";
    std::string destination = Files::join(Paths::dictionaries(), packName);
    DictionaryPackWriter writer(destination);
    auto info = DictionaryConverter::read(source, [&](const DictionaryImportEntry& entry) { writer.add(entry); }, &error);
    if (!info || !writer.finish(info->targetLanguage, info->definitionLanguage)) {
        Files::remove(destination);
        if (info) error = Files::baseName(source.main) + " held no entries this app could read.";
        return false;
    }
    DictionaryPack pack;
    pack.name = info->name.empty() ? source.defaultName() : info->name;
    pack.fileName = packName;
    pack.targetLanguage = info->targetLanguage;
    pack.definitionLanguage = info->definitionLanguage;
    insert(pack);
    return true;
}

int DictionaryPacks::addFromFolder(std::vector<std::string>& errors) {
    std::set<std::string> known;
    for (const auto& pack : all()) known.insert(pack.fileName);

    std::vector<std::string> names = Files::list(Paths::dictionaries());
    std::set<std::string> present(names.begin(), names.end());
    std::vector<std::string> paths;
    for (const auto& name : names) paths.push_back(Files::join(Paths::dictionaries(), name));

    int added = 0;
    for (const auto& source : DictionaryFormats::sources(paths)) {
        std::string name = Files::baseName(source.main);
        if (known.count(name)) continue;
        std::string error;
        bool ok;
        if (source.format == DictionaryFormat::Native) {
            ok = addNative(name, error);
        } else {
            // Already converted once: the pack beside it is registered instead.
            if (present.count(DictionaryFormats::stem(source.main) + ".converted.sqlite3")) continue;
            ok = convert(source, error);
        }
        if (ok) ++added; else errors.push_back(error);
    }
    return added;
}

int DictionaryPacks::addFile(const std::string& path, std::vector<std::string>& errors) {
    std::string directory = Files::directoryName(path);
    std::vector<std::string> siblings;
    for (const auto& name : Files::list(directory)) siblings.push_back(Files::join(directory, name));

    // The file may be a StarDict companion; the source it belongs to is what
    // gets copied, whole.
    const DictionarySource* chosen = nullptr;
    std::vector<DictionarySource> sources = DictionaryFormats::sources(siblings);
    for (const auto& source : sources) {
        bool companion = std::find(source.companions.begin(), source.companions.end(), path) != source.companions.end();
        if (source.main == path || companion) chosen = &source;
    }
    if (!chosen) {
        errors.push_back(Files::baseName(path) + " is not a dictionary this app can read.");
        return 0;
    }

    if (directory != Paths::dictionaries()) {
        std::vector<std::string> files = {chosen->main};
        files.insert(files.end(), chosen->companions.begin(), chosen->companions.end());
        for (const auto& file : files) {
            std::string destination = Files::join(Paths::dictionaries(), Files::baseName(file));
            if (Files::exists(destination)) continue;
            if (!Files::copy(file, destination)) {
                errors.push_back("Could not copy " + Files::baseName(file) + " into " + Paths::dictionaries() + ".");
                return 0;
            }
        }
    }
    return addFromFolder(errors);
}
