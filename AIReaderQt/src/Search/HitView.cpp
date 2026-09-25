#include "Search/HitView.hpp"

#include "Common/Tappable.hpp"
#include "Common/Ui.hpp"

#include <QVBoxLayout>

namespace HitView {

QWidget* create(const SearchHit& hit, bool showBook, std::function<void(const BookPosition&)> onTap) {
    const std::string& text = hit.excerpt;
    QString excerpt = Ui::escape(text.substr(0, hit.matchStart))
        + "<b>" + Ui::escape(text.substr(hit.matchStart, hit.matchEnd - hit.matchStart)) + "</b>"
        + Ui::escape(text.substr(hit.matchEnd));
    std::string where = "Chapter " + std::to_string(hit.chapter + 1);
    if (showBook) where = hit.bookTitle + " · " + where;

    BookPosition position{hit.bookId, hit.chapter, hit.offset};
    QWidget* row = onTap ? new Tappable([onTap, position] { onTap(position); }) : new QWidget;
    auto* column = new QVBoxLayout(row);
    column->setContentsMargins(6, 6, 6, 6);
    column->setSpacing(3);
    column->addWidget(Ui::rich(excerpt));
    column->addWidget(Ui::note(Ui::escape(where)));
    return row;
}

}  // namespace HitView
