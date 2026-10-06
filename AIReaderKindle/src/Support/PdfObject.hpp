#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/// A value in a PDF: what a file's objects and a page's content are made of.
/// A stream keeps its dictionary and where its still encoded bytes lie in
/// the file, so a large file is not held twice.
struct PdfObject {
    enum class Type { Null, Boolean, Number, String, Name, Array, Dictionary, Reference, Stream, Operator };
    using Members = std::vector<std::pair<std::string, PdfObject>>;

    Type type = Type::Null;
    /// A number, a boolean as 0 or 1, or the object number of a reference.
    double number = 0;
    /// A string's bytes, a name without its slash, or an operator.
    std::string text;
    std::vector<PdfObject> items;
    /// A dictionary's members, or a stream's dictionary.
    Members members;
    /// Where a stream's bytes lie in the file.
    size_t streamStart = 0;
    size_t streamLength = 0;

    bool is(Type kind) const { return type == kind; }
    bool isNull() const { return type == Type::Null; }
    bool isName(const std::string& name) const { return type == Type::Name && text == name; }
    int integer() const { return static_cast<int>(number); }
    /// A dictionary's or a stream's member, unresolved; null when missing.
    const PdfObject& member(const std::string& key) const;
};

/// Reads PDF syntax: the objects of a file, or the operands and operators
/// of a content stream. Malformed input yields what could be read, never
/// an exception.
class PdfLexer {
public:
    PdfLexer(const std::string& data, size_t position = 0) : data_(data), position_(position) {}

    /// The next object; a bare keyword comes back as an `Operator`.
    PdfObject next();
    /// True once only whitespace and comments are left.
    bool atEnd();
    size_t position() const { return position_; }
    void seek(size_t position) { position_ = position; }
    /// Steps over an inline image's data, from just after its `ID` to just
    /// after its `EI`.
    void skipInlineImage();

private:
    const std::string& data_;
    size_t position_;
    int depth_ = 0;

    void skipSpace();
    PdfObject number();
    PdfObject literalString();
    PdfObject hexString();
    PdfObject name();
    PdfObject array();
    PdfObject dictionary();
    std::string keyword();
    bool isDelimiter(size_t at) const;
};
