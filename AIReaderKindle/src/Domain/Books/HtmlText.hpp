#pragma once

#include <map>
#include <string>
#include <vector>

/// A styled stretch of text, as byte offsets into the plain text.
struct TextSpan {
    enum class Kind { Bold, Italic, Heading, Superscript, Subscript };
    int start;
    int end;
    Kind kind;
};

/// An illustration: where it sits in the text, as the byte offset of an
/// object-replacement character (U+FFFC) on a line of its own.
struct TextImage {
    int offset;
    /// As written in the markup; the loader resolves it and fills `bytes`.
    std::string source;
    std::string bytes;
};

/// A chapter reduced to what a page can show: its prose, with paragraphs
/// separated by newlines, the few styles worth keeping, and its pictures.
struct PlainText {
    std::string text;
    std::vector<TextSpan> spans;
    std::vector<TextImage> images;
    /// Where each element with an `id` begins, so a link into the chapter —
    /// a table of contents entry — can be followed to its place.
    std::map<std::string, int> anchors;
};

/// The character standing in for an illustration.
extern const char* const imagePlaceholder;

/// Turns an XHTML chapter into plain text with its illustrations marked.
namespace HtmlText {

PlainText plain(const std::string& markup);

}  // namespace HtmlText
