#include "Files.hpp"

#include "Text.hpp"

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <cstdio>

namespace Files {

std::optional<std::string> read(const std::string& path) {
    gchar* contents = nullptr;
    gsize length = 0;
    if (!g_file_get_contents(path.c_str(), &contents, &length, nullptr)) return std::nullopt;
    std::string result(contents, length);
    g_free(contents);
    return result;
}

bool write(const std::string& path, const std::string& contents) {
    return g_file_set_contents(path.c_str(), contents.data(), static_cast<gssize>(contents.size()), nullptr);
}

bool exists(const std::string& path) { return g_file_test(path.c_str(), G_FILE_TEST_EXISTS); }
bool isDirectory(const std::string& path) { return g_file_test(path.c_str(), G_FILE_TEST_IS_DIR); }
bool ensureDirectory(const std::string& path) { return g_mkdir_with_parents(path.c_str(), 0755) == 0; }
bool remove(const std::string& path) { return g_unlink(path.c_str()) == 0; }

bool copy(const std::string& from, const std::string& to) {
    FILE* source = std::fopen(from.c_str(), "rb");
    if (!source) return false;
    FILE* target = std::fopen(to.c_str(), "wb");
    if (!target) {
        std::fclose(source);
        return false;
    }
    char buffer[64 * 1024];
    bool ok = true;
    while (ok) {
        size_t count = std::fread(buffer, 1, sizeof buffer, source);
        if (count == 0) break;
        ok = std::fwrite(buffer, 1, count, target) == count;
    }
    ok = ok && !std::ferror(source);
    std::fclose(source);
    ok = std::fclose(target) == 0 && ok;
    if (!ok) g_unlink(to.c_str());  // never leave half a file behind
    return ok;
}

std::vector<std::string> list(const std::string& directory) {
    std::vector<std::string> names;
    GDir* dir = g_dir_open(directory.c_str(), 0, nullptr);
    if (!dir) return names;
    while (const gchar* name = g_dir_read_name(dir)) names.emplace_back(name);
    g_dir_close(dir);
    std::sort(names.begin(), names.end());
    return names;
}

std::string join(const std::string& directory, const std::string& name) {
    gchar* joined = g_build_filename(directory.c_str(), name.c_str(), nullptr);
    std::string result = joined;
    g_free(joined);
    return result;
}

std::string baseName(const std::string& path) {
    gchar* base = g_path_get_basename(path.c_str());
    std::string result = base;
    g_free(base);
    return result;
}

std::string directoryName(const std::string& path) {
    gchar* directory = g_path_get_dirname(path.c_str());
    std::string result = directory;
    g_free(directory);
    return result;
}

std::string extension(const std::string& path) {
    std::string name = baseName(path);
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return "";
    return Text::lower(name.substr(dot + 1));
}

std::string stem(const std::string& path) {
    std::string name = baseName(path);
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return name;
    return name.substr(0, dot);
}

}  // namespace Files
