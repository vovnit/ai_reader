#include "Menu/ContentsView.hpp"

#include "Common/Tappable.hpp"
#include "Common/Ui.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QScrollArea>
#include <QVBoxLayout>

ContentsView::ContentsView(Navigator& navigator, const ReaderFeature& reader, const ReaderLink& link)
    : Screen(navigator, "Contents") {
    auto* holder = new QWidget;
    QVBoxLayout* column = Ui::column(holder, 0);
    long long bookId = reader.book().id;
    int current = reader.contentsEntry();
    QWidget* currentRow = nullptr;

    const auto& entries = reader.contents();
    for (size_t i = 0; i < entries.size(); ++i) {
        const ContentsEntry& entry = entries[i];
        BookPosition position{bookId, entry.chapter, entry.offset};
        auto* row = new Tappable([jump = link.jump, position] { jump(position); });
        auto* line = new QHBoxLayout(row);
        // Each level of nesting steps in by an indent.
        line->setContentsMargins(8 + 20 * entry.depth, 10, 8, 10);
        line->addWidget(Ui::label(entry.title), 1);
        if (static_cast<int>(i) == current) {
            line->addWidget(new QLabel("✓"));
            currentRow = row;
        }
        column->addWidget(row);
        column->addWidget(Ui::separator());
    }
    if (entries.empty()) column->addWidget(Ui::label("This book has no table of contents."));

    QScrollArea* scroller = Ui::scrolled(holder);
    setBody(scroller);
    // A long list opens at what is being read rather than at the top.
    if (currentRow) {
        QPointer<QWidget> row = currentRow;
        Ui::later([scroller, row] {
            if (row) scroller->ensureWidgetVisible(row, 0, scroller->viewport()->height() / 3);
        });
    }
}
