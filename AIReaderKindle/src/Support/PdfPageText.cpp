#include "PdfPageText.hpp"

#include <glib.h>

#include <algorithm>
#include <cmath>

namespace PdfPageText {

namespace {

/// An affine transformation, as PDF writes one: `[a b c d e f]`.
struct Matrix {
    double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;

    /// This transformation, followed by `next`.
    Matrix then(const Matrix& next) const {
        return {a * next.a + b * next.c, a * next.b + b * next.d,
                c * next.a + d * next.c, c * next.b + d * next.d,
                e * next.a + f * next.c + next.e, e * next.b + f * next.d + next.f};
    }
};

Matrix translation(double x, double y) {
    return {1, 0, 0, 1, x, y};
}

/// Runs content streams, keeping the graphics and text state the PDF
/// describes, and collects what they show into lines.
class Reader {
public:
    Reader(const PdfDocument& document, double top, FontCache& fonts) : document_(document), top_(top), fonts_(fonts) {}

    void run(const std::string& content, const PdfObject& resources, int depth);

    std::vector<PdfLine> finished() {
        flush();
        return std::move(lines_);
    }

private:
    struct State {
        Matrix ctm;
        const PdfFont* font = nullptr;
        double fontSize = 0;
        double charSpacing = 0;
        double wordSpacing = 0;
        double scale = 1;
        double leading = 0;
        double rise = 0;
    };

    const PdfDocument& document_;
    double top_;
    FontCache& fonts_;
    PdfFont missing_;
    State state_;
    std::vector<State> saved_;
    Matrix text_;
    Matrix line_;
    std::vector<PdfLine> lines_;
    PdfLine current_;
    bool open_ = false;

    const PdfFont* font(const PdfObject& resources, const std::string& name);
    void show(const std::string& bytes);
    void place(const std::string& text, double x, double y, double size, double advance);
    void flush();
    void nextLine(double x, double y) {
        line_ = translation(x, y).then(line_);
        text_ = line_;
    }
};

double number(const std::vector<PdfObject>& operands, size_t count, size_t index) {
    return operands.size() >= count ? operands[operands.size() - count + index].number : 0;
}

Matrix matrix(const std::vector<PdfObject>& values) {
    if (values.size() < 6) return {};
    const size_t at = values.size() - 6;
    return {values[at].number, values[at + 1].number, values[at + 2].number, values[at + 3].number, values[at + 4].number, values[at + 5].number};
}

void Reader::run(const std::string& content, const PdfObject& resources, int depth) {
    PdfLexer lexer(content);
    std::vector<PdfObject> operands;
    while (!lexer.atEnd()) {
        PdfObject token = lexer.next();
        if (!token.is(PdfObject::Type::Operator)) {
            operands.push_back(std::move(token));
            continue;
        }
        const std::string& op = token.text;
        const std::string shown = operands.empty() ? "" : operands.back().text;
        if (op == "BT") {
            text_ = line_ = Matrix();
        } else if (op == "Tj") {
            show(shown);
        } else if (op == "TJ" && !operands.empty()) {
            for (const auto& item : operands.back().items) {
                if (item.is(PdfObject::Type::String)) show(item.text);
                else text_ = translation(-item.number / 1000 * state_.fontSize * state_.scale, 0).then(text_);
            }
        } else if (op == "'" || op == "\"") {
            if (op == "\"") {
                state_.wordSpacing = number(operands, 3, 0);
                state_.charSpacing = number(operands, 3, 1);
            }
            nextLine(0, -state_.leading);
            show(shown);
        } else if (op == "Td" || op == "TD") {
            if (op == "TD") state_.leading = -number(operands, 2, 1);
            nextLine(number(operands, 2, 0), number(operands, 2, 1));
        } else if (op == "T*") {
            nextLine(0, -state_.leading);
        } else if (op == "Tm") {
            text_ = line_ = matrix(operands);
        } else if (op == "Tf" && operands.size() >= 2) {
            state_.font = font(resources, operands[operands.size() - 2].text);
            state_.fontSize = operands.back().number;
        } else if (op == "Tc") {
            state_.charSpacing = number(operands, 1, 0);
        } else if (op == "Tw") {
            state_.wordSpacing = number(operands, 1, 0);
        } else if (op == "Tz") {
            state_.scale = number(operands, 1, 0) / 100;
        } else if (op == "TL") {
            state_.leading = number(operands, 1, 0);
        } else if (op == "Ts") {
            state_.rise = number(operands, 1, 0);
        } else if (op == "q") {
            if (saved_.size() < 64) saved_.push_back(state_);
        } else if (op == "Q") {
            if (!saved_.empty()) {
                state_ = saved_.back();
                saved_.pop_back();
            }
        } else if (op == "cm") {
            state_.ctm = matrix(operands).then(state_.ctm);
        } else if (op == "Do" && !operands.empty() && depth < 8) {
            // A form is a content stream of its own, drawn where it is placed.
            const PdfObject& form = document_.get(document_.get(resources, "XObject"), operands.back().text);
            if (form.is(PdfObject::Type::Stream) && document_.get(form, "Subtype").isName("Form")) {
                State before = state_;
                size_t depthBefore = saved_.size();
                Matrix textBefore = text_;
                Matrix lineBefore = line_;
                state_.ctm = matrix(document_.get(form, "Matrix").items).then(state_.ctm);
                const PdfObject& own = document_.get(form, "Resources");
                run(document_.contents(form), own.is(PdfObject::Type::Dictionary) ? own : resources, depth + 1);
                state_ = before;
                saved_.resize(std::min(saved_.size(), depthBefore));
                text_ = textBefore;
                line_ = lineBefore;
            }
        } else if (op == "BI") {
            while (!lexer.atEnd()) {
                PdfObject word = lexer.next();
                if (word.is(PdfObject::Type::Operator) && word.text == "ID") break;
            }
            lexer.skipInlineImage();
        }
        operands.clear();
    }
}

const PdfFont* Reader::font(const PdfObject& resources, const std::string& name) {
    const PdfObject& dictionary = document_.get(document_.get(resources, "Font"), name);
    if (!dictionary.is(PdfObject::Type::Dictionary)) return &missing_;
    auto found = fonts_.find(&dictionary);
    if (found != fonts_.end()) return &found->second;
    return &fonts_.emplace(&dictionary, PdfFont(document_, dictionary)).first->second;
}

void Reader::show(const std::string& bytes) {
    const PdfFont& font = state_.font ? *state_.font : missing_;
    for (const auto& glyph : font.glyphs(bytes)) {
        Matrix placed = text_.then(state_.ctm);
        Matrix origin = translation(0, state_.rise).then(placed);
        double size = std::abs(state_.fontSize) * std::hypot(placed.c, placed.d);
        double advance = (glyph.width / 1000 * state_.fontSize + state_.charSpacing + (glyph.isSpace ? state_.wordSpacing : 0)) * state_.scale;
        place(glyph.text, origin.e, origin.f, size, advance * std::hypot(placed.a, placed.b));
        text_ = translation(advance, 0).then(text_);
    }
}

void Reader::place(const std::string& text, double x, double y, double size, double advance) {
    double down = top_ - y;
    bool blank = true;
    for (const char* p = text.c_str(); *p && blank; p = g_utf8_next_char(p)) blank = g_unichar_isspace(g_utf8_get_char(p));
    if (open_) {
        double em = std::max(current_.size, size);
        // The same baseline, give or take a superscript, and not far back.
        if (std::abs(down - current_.y) <= em * 0.5 && x >= current_.right - em * 1.5) {
            // A no-break space is a space already.
            bool spaced = !current_.text.empty()
                && (current_.text.back() == ' ' || current_.text.compare(current_.text.size() - std::min<size_t>(2, current_.text.size()), 2, "\u00A0") == 0);
            if (x - current_.right > size * 0.15 && !spaced && !blank) current_.text += ' ';
            if (!(blank && spaced)) current_.text += text;
            current_.right = std::max(current_.right, x + advance);
            current_.size = std::max(current_.size, size);
            return;
        }
        flush();
    }
    if (blank) return;
    current_ = {text, x, x + advance, down, size};
    open_ = true;
}

void Reader::flush() {
    if (!open_) return;
    open_ = false;
    size_t end = current_.text.find_last_not_of(' ');
    if (end == std::string::npos) return;
    current_.text.erase(end + 1);
    lines_.push_back(std::move(current_));
}

}  // namespace

std::vector<PdfLine> lines(const PdfDocument& document, const PdfDocument::Page& page, FontCache& fonts) {
    std::string content;
    const PdfObject& contents = document.get(page.dictionary, "Contents");
    if (contents.is(PdfObject::Type::Array)) {
        for (const auto& part : contents.items) content += document.contents(document.resolve(part)) + "\n";
    } else {
        content = document.contents(contents);
    }
    Reader reader(document, page.top, fonts);
    reader.run(content, page.resources, 0);
    return reader.finished();
}

}  // namespace PdfPageText
