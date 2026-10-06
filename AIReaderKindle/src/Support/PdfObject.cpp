#include "PdfObject.hpp"

#include <cstdlib>

namespace {

bool isSpace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\0';
}

bool isDelimiterCharacter(char c) {
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%';
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/// Nesting deeper than this is not a book's; it is a broken or hostile file.
constexpr int depthLimit = 64;

}  // namespace

const PdfObject& PdfObject::member(const std::string& key) const {
    static const PdfObject null;
    for (const auto& entry : members) {
        if (entry.first == key) return entry.second;
    }
    return null;
}

bool PdfLexer::isDelimiter(size_t at) const {
    return at >= data_.size() || isSpace(data_[at]) || isDelimiterCharacter(data_[at]);
}

void PdfLexer::skipSpace() {
    while (position_ < data_.size()) {
        char c = data_[position_];
        if (isSpace(c)) {
            ++position_;
        } else if (c == '%') {
            while (position_ < data_.size() && data_[position_] != '\n' && data_[position_] != '\r') ++position_;
        } else {
            break;
        }
    }
}

bool PdfLexer::atEnd() {
    skipSpace();
    return position_ >= data_.size();
}

PdfObject PdfLexer::next() {
    skipSpace();
    PdfObject object;
    if (position_ >= data_.size()) return object;
    char c = data_[position_];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') return number();
    if (c == '(') return literalString();
    if (c == '/') return name();
    if (c == '[') return array();
    if (c == '<') return position_ + 1 < data_.size() && data_[position_ + 1] == '<' ? dictionary() : hexString();
    if (isDelimiterCharacter(c)) {
        // A stray `]`, `>>` or brace: handed back as an operator to be ignored.
        ++position_;
        if (c == '>' && position_ < data_.size() && data_[position_] == '>') ++position_;
        object.type = PdfObject::Type::Operator;
        object.text = std::string(1, c);
        return object;
    }
    std::string word = keyword();
    if (word == "true" || word == "false") {
        object.type = PdfObject::Type::Boolean;
        object.number = word == "true";
    } else if (word != "null") {
        object.type = PdfObject::Type::Operator;
        object.text = word;
    }
    return object;
}

std::string PdfLexer::keyword() {
    size_t start = position_;
    while (!isDelimiter(position_)) ++position_;
    if (position_ == start) ++position_;  // never stall on an unexpected byte
    return data_.substr(start, position_ - start);
}

PdfObject PdfLexer::number() {
    size_t start = position_;
    if (data_[position_] == '-' || data_[position_] == '+') ++position_;
    bool whole = true;
    while (position_ < data_.size() && ((data_[position_] >= '0' && data_[position_] <= '9') || data_[position_] == '.')) {
        whole = whole && data_[position_] != '.';
        ++position_;
    }
    PdfObject object;
    object.type = PdfObject::Type::Number;
    std::string digits = data_.substr(start, position_ - start);
    object.number = digits == "-" || digits == "+" || digits == "." ? 0 : std::strtod(digits.c_str(), nullptr);
    // `12 0 R` is a reference to object 12.
    if (whole && data_[start] >= '0' && data_[start] <= '9') {
        size_t after = position_;
        skipSpace();
        size_t generationStart = position_;
        while (position_ < data_.size() && data_[position_] >= '0' && data_[position_] <= '9') ++position_;
        if (position_ > generationStart && isDelimiter(position_)) {
            skipSpace();
            if (position_ < data_.size() && data_[position_] == 'R' && isDelimiter(position_ + 1)) {
                ++position_;
                object.type = PdfObject::Type::Reference;
                return object;
            }
        }
        position_ = after;
    }
    return object;
}

PdfObject PdfLexer::literalString() {
    PdfObject object;
    object.type = PdfObject::Type::String;
    ++position_;
    int nesting = 1;
    while (position_ < data_.size()) {
        char c = data_[position_++];
        if (c == '(') {
            ++nesting;
        } else if (c == ')') {
            if (--nesting == 0) break;
        } else if (c == '\\' && position_ < data_.size()) {
            char escaped = data_[position_++];
            switch (escaped) {
            case 'n': object.text += '\n'; continue;
            case 'r': object.text += '\r'; continue;
            case 't': object.text += '\t'; continue;
            case 'b': object.text += '\b'; continue;
            case 'f': object.text += '\f'; continue;
            case '\r':
                if (position_ < data_.size() && data_[position_] == '\n') ++position_;
                continue;
            case '\n': continue;
            default: break;
            }
            if (escaped >= '0' && escaped <= '7') {
                int value = escaped - '0';
                for (int digits = 1; digits < 3 && position_ < data_.size() && data_[position_] >= '0' && data_[position_] <= '7'; ++digits) {
                    value = value * 8 + (data_[position_++] - '0');
                }
                object.text += static_cast<char>(value & 0xFF);
                continue;
            }
            object.text += escaped;
            continue;
        } else if (c == '\r') {
            // An end of line inside a string reads as a bare line feed.
            if (position_ < data_.size() && data_[position_] == '\n') ++position_;
            c = '\n';
        }
        object.text += c;
    }
    return object;
}

PdfObject PdfLexer::hexString() {
    PdfObject object;
    object.type = PdfObject::Type::String;
    ++position_;
    int high = -1;
    while (position_ < data_.size() && data_[position_] != '>') {
        int value = hexValue(data_[position_++]);
        if (value < 0) continue;
        if (high < 0) {
            high = value;
        } else {
            object.text += static_cast<char>(high * 16 + value);
            high = -1;
        }
    }
    if (high >= 0) object.text += static_cast<char>(high * 16);
    ++position_;
    return object;
}

PdfObject PdfLexer::name() {
    PdfObject object;
    object.type = PdfObject::Type::Name;
    ++position_;
    while (!isDelimiter(position_)) {
        char c = data_[position_++];
        if (c == '#' && position_ + 1 < data_.size() && hexValue(data_[position_]) >= 0 && hexValue(data_[position_ + 1]) >= 0) {
            c = static_cast<char>(hexValue(data_[position_]) * 16 + hexValue(data_[position_ + 1]));
            position_ += 2;
        }
        object.text += c;
    }
    return object;
}

PdfObject PdfLexer::array() {
    PdfObject object;
    object.type = PdfObject::Type::Array;
    ++position_;
    if (++depth_ > depthLimit) {
        --depth_;
        return object;
    }
    while (!atEnd()) {
        if (data_[position_] == ']') {
            ++position_;
            break;
        }
        object.items.push_back(next());
    }
    --depth_;
    return object;
}

PdfObject PdfLexer::dictionary() {
    PdfObject object;
    object.type = PdfObject::Type::Dictionary;
    position_ += 2;
    if (++depth_ > depthLimit) {
        --depth_;
        return object;
    }
    while (!atEnd()) {
        if (data_.compare(position_, 2, ">>") == 0) {
            position_ += 2;
            break;
        }
        PdfObject key = next();
        if (!key.is(PdfObject::Type::Name)) continue;
        // A key with no value before the end reads as null.
        skipSpace();
        if (data_.compare(position_, 2, ">>") == 0) continue;
        object.members.emplace_back(key.text, next());
    }
    --depth_;
    return object;
}

void PdfLexer::skipInlineImage() {
    // The data is binary; it ends at `EI` standing alone.
    if (position_ < data_.size() && isSpace(data_[position_])) ++position_;
    while (position_ + 1 < data_.size()) {
        if (data_[position_] == 'E' && data_[position_ + 1] == 'I' && (position_ == 0 || isSpace(data_[position_ - 1]))
            && isDelimiter(position_ + 2)) {
            position_ += 2;
            return;
        }
        ++position_;
    }
    position_ = data_.size();
}
