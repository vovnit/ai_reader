#include "Paths.hpp"

#include "../Support/Files.hpp"

#include <glib.h>

#ifndef AIREADER_DATA_DIR
#define AIREADER_DATA_DIR "."
#endif

namespace Paths {

static std::string environment(const char* name) {
    const gchar* value = g_getenv(name);
    return value ? value : "";
}

std::string home() {
    std::string configured = environment("AIREADER_HOME");
    if (!configured.empty()) return configured;
    return Files::join(g_get_home_dir(), ".aireader");
}

std::string books() { return Files::join(home(), "books"); }
std::string dictionaries() { return Files::join(home(), "dictionaries"); }
std::string database() { return Files::join(home(), "library.sqlite3"); }
std::string settings() { return Files::join(home(), "settings.ini"); }
std::string ankiCards() { return Files::join(home(), "anki-cards.txt"); }

std::vector<std::string> bookFolders() {
    std::vector<std::string> folders = {books()};
    // The Kindle's own documents folder is where a computer drops files.
    if (onKindle()) folders.push_back("/mnt/us/documents");
    return folders;
}

std::string browseRoot() {
    return onKindle() ? "/mnt/us" : g_get_home_dir();
}

std::string bundledDictionary() {
    std::string configured = environment("AIREADER_DATA_DIR");
    return Files::join(configured.empty() ? AIREADER_DATA_DIR : configured, "dictionary.sqlite3");
}

bool onKindle() {
    return environment("AIREADER_KINDLE") == "1" || Files::isDirectory("/mnt/us");
}

void prepare() {
    Files::ensureDirectory(books());
    Files::ensureDirectory(dictionaries());
}

}  // namespace Paths
