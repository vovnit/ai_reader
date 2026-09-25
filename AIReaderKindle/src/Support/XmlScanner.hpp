#pragma once

#include <functional>
#include <map>
#include <string>

/// Walks XML (or the XHTML inside an EPUB) and reports tags and text to
/// closures, so small parsers can be written without a parser class each time.
/// Element and attribute names are reported lowercased and without their
/// namespace prefix; entities in text and attributes are decoded.
namespace XmlScanner {

using Attributes = std::map<std::string, std::string>;

struct Handlers {
    std::function<void(const std::string& name, const Attributes& attributes)> onStart;
    std::function<void(const std::string& name)> onEnd;
    std::function<void(const std::string& text)> onText;
};

void scan(const std::string& markup, const Handlers& handlers);

std::string decodeEntities(const std::string& text);

}  // namespace XmlScanner
