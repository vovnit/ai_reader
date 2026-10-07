#include "Glossary.hpp"

#include "../Domain/AI/GlossaryPrompt.hpp"
#include "../Support/Files.hpp"
#include "../Support/Text.hpp"
#include "ChatApi.hpp"
#include "Paths.hpp"

#include <fstream>
#include <mutex>

namespace Glossary {

std::string name(const Book& book) {
    return book.title + " glossary";
}

std::string path(const Book& book) {
    std::string file = name(book);
    for (char& c : file) {
        if (std::string("\\/:*?\"<>|").find(c) != std::string::npos) c = ' ';
    }
    return Files::join(Paths::dictionaries(), Text::trim(file) + ".tsv");
}

std::set<std::string> definedForms(const Book& book) {
    std::set<std::string> forms;
    for (const auto& line : Text::split(Files::read(path(book)).value_or(""), '\n')) {
        if (line.empty() || line[0] == '#') continue;
        forms.insert(line.substr(0, line.find('\t')));
    }
    return forms;
}

std::map<std::string, std::string> define(const AiSettings& settings, const std::vector<BookWord>& words) {
    std::vector<ChatMessage> messages = {
        ChatMessage::system(GlossaryPrompt::system(settings.language)),
        ChatMessage::user(GlossaryPrompt::question(words)),
    };
    ChatMessage reply = ChatApi::chat(settings, messages, Json::array(), true, GlossaryPrompt::maxTokens);
    auto definitions = GlossaryPrompt::definitions(reply.content.value_or(""), words);
    if (!definitions) throw ChatApi::Error("The model's answer could not be read.");
    return *definitions;
}

void append(const Book& book, const AiSettings& settings, const std::map<std::string, std::string>& definitions) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    Files::ensureDirectory(Paths::dictionaries());
    bool fresh = !Files::exists(path(book));
    std::ofstream file(path(book), std::ios::app | std::ios::binary);
    if (fresh) {
        file << "# Glossary of “" << book.title << "”: every word as the book writes it, and what it means there.\n"
             << "# In " << settings.language << ", written by " << settings.model << " in AIReader.\n";
    }
    for (const auto& [form, definition] : definitions) file << form << '\t' << definition << '\n';
}

void install(DictionaryPacks& packs, const Book& book) {
    if (!Files::exists(path(book))) return;
    for (const auto& pack : packs.all()) {
        if (pack.name == name(book)) packs.remove(pack);
    }
    std::vector<std::string> errors;
    packs.addFile(path(book), errors);
}

}  // namespace Glossary
