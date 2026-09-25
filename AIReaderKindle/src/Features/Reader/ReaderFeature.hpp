#pragma once

#include "../../Domain/Books/Book.hpp"
#include "../../Domain/Books/BookDocument.hpp"
#include "../../Domain/Reading/Paginator.hpp"
#include "../../Domain/Reading/ReadingStyle.hpp"
#include "../../Domain/Reading/WordContext.hpp"
#include "../../Services/BookCorpus.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// Reading one book: load its chapters, lay the current one out as pages,
/// remember the position, and turn taps into page turns or word lookups.
/// Holds the corpus — this book and its group — that searches run over.
class ReaderFeature {
public:
    enum class Status { Loading, Loaded, Failed };

    struct Tap {
        enum class Kind { Nothing, Turned, Word };
        Kind kind = Kind::Nothing;
        WordContext::Selection selection;
    };

    ReaderFeature(Env& env, Book book);
    ~ReaderFeature();
    ReaderFeature(const ReaderFeature&) = delete;
    ReaderFeature& operator=(const ReaderFeature&) = delete;

    /// Reads the EPUB on a worker thread.
    void load();
    /// The Pango context pages are laid out with, from the page widget, and
    /// how much denser than a desktop the screen is.
    void setContext(PangoContext* context, double scale);
    /// The page widget's size, margins included.
    void setPageSize(int width, int height);
    void setStyle(const ReadingStyle& style);

    Status status() const { return status_; }
    const std::string& error() const { return error_; }
    const Book& book() const { return book_; }
    const ReadingStyle& style() const { return style_; }
    std::string language() const;

    const PageLayout& layout() const { return layout_; }
    const Page* page() const;
    int marginPixels() const;
    int chapterIndex() const { return chapter_; }
    int chapterCount() const;
    int pageIndex() const { return page_; }
    int pageCount() const { return static_cast<int>(layout_.pages.size()); }
    /// The text of the page on screen, so the menu's chat can be about it.
    std::string pageText() const;
    /// The table of contents, and which of its entries the page on screen
    /// falls under (-1 before the first).
    const std::vector<ContentsEntry>& contents() const;
    int contentsEntry() const;
    /// The end of the page on screen: how far the reader has got.
    BookPosition position() const;
    /// This book and the others in its group, searchable from any thread.
    const std::shared_ptr<BookCorpus>& corpus() const { return corpus_; }
    /// The corpus with the position: what lookups and conversations search.
    ReadingScope scope() const { return {corpus_, position()}; }
    /// The group the book is in, if any.
    std::optional<BookGroup> group() const;

    void next();
    void previous();
    /// Turns to the page holding `offset` of `chapter`.
    void goTo(int chapter, int offset);
    Tap tapAt(double x, double y);

    /// Fired whenever something drawn or shown may have changed.
    std::function<void()> onChange;

private:
    Env& env_;
    Book book_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    Status status_ = Status::Loading;
    std::string error_;
    std::optional<BookDocument> document_;
    PangoContext* context_ = nullptr;
    double scale_ = 1;
    int width_ = 0;
    int height_ = 0;
    ReadingStyle style_;
    int chapter_ = 0;
    int page_ = 0;
    PageLayout layout_;
    WordContext words_;
    std::shared_ptr<BookCorpus> corpus_;

    void relayout(int anchorOffset);
    void showChapter(int chapter, int anchorOffset);
    void savePosition();
    void changed();
};
