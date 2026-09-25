#include "ReaderFeature.hpp"

#include "../../Services/EpubLoader.hpp"
#include "../../Support/Async.hpp"

#include <climits>
#include <cstdlib>

ReaderFeature::ReaderFeature(Env& env, Book book)
    : env_(env), book_(std::move(book)), style_(env.settings.style()) {
    chapter_ = book_.readingChapter;
    // The open book is searched first, then the rest of its group.
    std::vector<Book> books = {book_};
    if (book_.groupId) {
        for (const auto& other : env_.library.inGroup(book_.groupId)) {
            if (other.id != book_.id) books.push_back(other);
        }
    }
    corpus_ = std::make_shared<BookCorpus>(books);
}

ReaderFeature::~ReaderFeature() {
    if (context_) g_object_unref(context_);
}

void ReaderFeature::load() {
    if (document_) return;
    struct Loaded {
        std::optional<BookDocument> document;
        std::string error;
    };
    std::string path = book_.path;
    Async::run<Loaded>(
        [path] {
            Loaded loaded;
            loaded.document = EpubLoader::load(path, &loaded.error);
            return loaded;
        },
        [this](Loaded loaded) {
            if (!loaded.document) {
                status_ = Status::Failed;
                error_ = loaded.error;
                changed();
                return;
            }
            document_ = std::move(loaded.document);
            status_ = Status::Loaded;
            std::vector<std::string> chapters;
            for (const auto& chapter : document_->chapters) chapters.push_back(chapter.text);
            corpus_->provide(book_.id, std::move(chapters));
            if (!document_->language.empty() && document_->language != book_.language) {
                book_.language = document_->language;
                env_.library.saveLanguage(book_.id, book_.language);
            }
            chapter_ = std::min(std::max(chapter_, 0), chapterCount() - 1);
            // A place that came from another device is found in this text
            // now that it is here.
            if (book_.placePending && book_.place) {
                book_.readingOffset = book_.place->resolve(document_->chapter(chapter_).text);
                book_.placePending = false;
            }
            relayout(book_.readingOffset);
        },
        alive_);
}

void ReaderFeature::setContext(PangoContext* context, double scale) {
    if (context_) g_object_unref(context_);
    context_ = context;
    if (context_) g_object_ref(context_);
    scale_ = scale;
}

void ReaderFeature::setPageSize(int width, int height) {
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    relayout(page() ? page()->start : book_.readingOffset);
}

void ReaderFeature::setStyle(const ReadingStyle& style) {
    if (style == style_) return;
    style_ = style;
    relayout(page() ? page()->start : book_.readingOffset);
}

std::string ReaderFeature::language() const {
    return document_ && !document_->language.empty() ? document_->language : book_.language;
}

const Page* ReaderFeature::page() const {
    if (page_ < 0 || page_ >= pageCount()) return nullptr;
    return &layout_.pages[page_];
}

int ReaderFeature::marginPixels() const {
    return static_cast<int>(style_.margin * scale_);
}

int ReaderFeature::chapterCount() const {
    return document_ ? static_cast<int>(document_->chapters.size()) : 0;
}

std::string ReaderFeature::pageText() const {
    const Page* current = page();
    if (!current || !document_) return "";
    return document_->chapter(chapter_).text.substr(current->start, current->end - current->start);
}

const std::vector<ContentsEntry>& ReaderFeature::contents() const {
    static const std::vector<ContentsEntry> none;
    return document_ ? document_->contents : none;
}

int ReaderFeature::contentsEntry() const {
    const Page* current = page();
    return document_ ? document_->contentsEntryAt(chapter_, current ? current->start : 0) : -1;
}

BookPosition ReaderFeature::position() const {
    const Page* current = page();
    return {book_.id, chapter_, current ? current->end : book_.readingOffset};
}

std::optional<BookGroup> ReaderFeature::group() const {
    return book_.groupId ? env_.groups.find(book_.groupId) : std::nullopt;
}

/// Lays the current chapter out again and lands on the page holding `anchorOffset`.
void ReaderFeature::relayout(int anchorOffset) {
    if (!document_ || !context_ || width_ < 1 || height_ < 1) return;
    const PlainText& chapter = document_->chapter(chapter_);
    int margin = marginPixels();
    layout_ = Paginator::paginate(
        context_, chapter, style_,
        std::max(width_ - 2 * margin, 1), std::max(height_ - 2 * margin, 1),
        scale_, language());
    words_ = WordContext(chapter.text, language());
    page_ = layout_.pageContaining(anchorOffset);
    savePosition();
    changed();
}

void ReaderFeature::showChapter(int chapter, int anchorOffset) {
    chapter_ = chapter;
    relayout(anchorOffset);
}

void ReaderFeature::next() {
    if (page_ + 1 < pageCount()) {
        ++page_;
        savePosition();
        changed();
    } else if (chapter_ + 1 < chapterCount()) {
        showChapter(chapter_ + 1, 0);
    }
}

void ReaderFeature::previous() {
    if (page_ > 0) {
        --page_;
        savePosition();
        changed();
    } else if (chapter_ > 0) {
        showChapter(chapter_ - 1, INT_MAX);
    }
}

void ReaderFeature::goTo(int chapter, int offset) {
    if (chapter < 0 || chapter >= chapterCount()) return;
    showChapter(chapter, offset);
}

ReaderFeature::Tap ReaderFeature::tapAt(double x, double y) {
    Tap tap;
    const Page* current = page();
    if (!current || !layout_.layout) return tap;

    auto turn = [&] {
        if (x < width_ / 2.0) previous(); else next();
        tap.kind = Tap::Kind::Turned;
        return tap;
    };

    int margin = marginPixels();
    int lx = static_cast<int>((x - margin) * PANGO_SCALE);
    int ly = static_cast<int>((y - margin) * PANGO_SCALE) + current->top;
    if (ly < current->top || ly > current->top + current->height) return turn();

    int index = 0;
    int trailing = 0;
    pango_layout_xy_to_index(layout_.layout, lx, ly, &index, &trailing);
    // `xy_to_index` snaps to the nearest character; only a tap that lands on
    // one counts as a tap on it.
    PangoRectangle box;
    pango_layout_index_to_pos(layout_.layout, index, &box);
    int left = std::min(box.x, box.x + box.width);
    int right = std::max(box.x, box.x + box.width);
    if (lx < left || lx > right || ly < box.y || ly > box.y + box.height) return turn();

    auto selection = words_.selectionAt(index);
    if (!selection) return tap;
    tap.kind = Tap::Kind::Word;
    tap.selection = *selection;
    return tap;
}

void ReaderFeature::savePosition() {
    const Page* current = page();
    if (!current) return;
    book_.readingChapter = chapter_;
    book_.readingOffset = current->start;
    book_.place = ReadingPlace::at(chapter_, document_->chapter(chapter_).text, current->start);
    book_.placePending = false;
    env_.library.savePosition(book_.id, chapter_, current->start, book_.place);
}

void ReaderFeature::changed() {
    if (onChange) onChange();
}
