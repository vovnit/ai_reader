#include "Library/LibraryView.hpp"

#include "Common/Navigator.hpp"
#include "Common/Tappable.hpp"
#include "Common/Ui.hpp"
#include "Reader/ReaderView.hpp"
#include "Services/EpubLoader.hpp"
#include "Services/Paths.hpp"
#include "Settings/SettingsView.hpp"
#include "Words/WordsView.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int coverWidth = 60;
constexpr int coverHeight = 90;

/// Decodes the book's cover into `image` once the list is on screen.
void loadCover(QLabel* image, const std::string& path) {
    QPointer<QLabel> target = image;
    Ui::later([target, path] {
        if (!target) return;
        auto bytes = EpubLoader::cover(path);
        QImage cover;
        if (!bytes || !cover.loadFromData(reinterpret_cast<const uchar*>(bytes->data()), static_cast<int>(bytes->size()))) return;
        qreal ratio = target->devicePixelRatioF();
        QPixmap pixmap = QPixmap::fromImage(cover.scaled(QSize(coverWidth, coverHeight) * ratio,
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation));
        pixmap.setDevicePixelRatio(ratio);
        target->setPixmap(pixmap);
    });
}

}  // namespace

LibraryView::LibraryView(Env& env, Navigator& navigator)
    : Screen(navigator, "Library", ""), env_(env), feature_(env) {
    auto* holder = new QWidget;
    list_ = Ui::column(holder, 6);
    setBody(Ui::scrolled(holder));

    addAction("Settings", [this] { this->navigator.push(new SettingsView(env_, this->navigator)); });
    addAction("Words", [this] { this->navigator.push(new WordsView(env_, this->navigator, 0)); });
    addAction("Refresh", [this] { feature_.refresh(); });
    addAction("Add…", [this] { addBook(); });

    feature_.onChange = [this] { render(); };
    feature_.refresh();
    feature_.sync();
}

void LibraryView::returned() {
    feature_.refresh();
    feature_.sync();
}

void LibraryView::render() {
    Ui::clear(list_);
    if (feature_.books().empty()) {
        list_->addWidget(Ui::rich("No books yet.<br><br>" + Ui::small(
            "Choose Add to pick an <tt>.epub</tt>, or copy files into<br><tt>" + Ui::escape(Paths::books())
            + "</tt><br>and choose Refresh.")));
    }
    auto shelve = [this](const std::vector<Book>& books) {
        for (const auto& book : books) {
            list_->addWidget(row(book));
            list_->addWidget(Ui::separator());
        }
    };
    // Each group under its name, then the books in none.
    for (const auto& group : feature_.groups()) {
        std::vector<Book> books = feature_.booksIn(group.id);
        list_->addWidget(heading(group, books.size()));
        shelve(books);
    }
    shelve(feature_.booksIn(0));
}

QWidget* LibraryView::row(const Book& book) {
    auto* line = new QWidget;
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* open = new Tappable([this, book] { navigator.push(new ReaderView(env_, navigator, book)); });
    auto* content = new QHBoxLayout(open);
    content->setContentsMargins(6, 6, 6, 6);
    content->setSpacing(12);
    auto* cover = new QLabel;
    cover->setFixedSize(coverWidth, coverHeight);
    cover->setAlignment(Qt::AlignCenter);
    loadCover(cover, book.path);
    content->addWidget(cover);
    QString text = "<b>" + Ui::escape(book.title) + "</b>";
    if (!book.author.empty()) text += "<br>" + Ui::small(Ui::escape(book.author));
    content->addWidget(Ui::rich(text), 1);
    layout->addWidget(open, 1);

    auto* group = new QPushButton("Group");
    QObject::connect(group, &QPushButton::clicked, this, [this, book, group] { pickGroup(book, group); });
    auto* remove = new QPushButton("✕");
    remove->setToolTip("Remove the book");
    QObject::connect(remove, &QPushButton::clicked, this, [this, book] {
        if (Ui::confirm(this, "Remove “" + book.title + "”?",
                        "The book file is deleted. Words looked up in it are kept.", "Remove")) {
            feature_.remove(book);
        }
    });
    layout->addWidget(group);
    layout->addWidget(remove);
    return line;
}

QWidget* LibraryView::heading(const BookGroup& group, size_t count) {
    auto* line = new QWidget;
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(6, 12, 0, 0);
    layout->addWidget(Ui::rich("<b>" + Ui::escape(group.name) + "</b>&nbsp;&nbsp;"
                               + Ui::small(QString::number(count) + (count == 1 ? " book" : " books"))), 1);
    auto* dissolve = new QPushButton("✕");
    dissolve->setToolTip("Dissolve the group");
    QObject::connect(dissolve, &QPushButton::clicked, this, [this, group] {
        if (Ui::confirm(this, "Dissolve “" + group.name + "”?", "The books stay on the shelf, on their own.", "Dissolve")) {
            feature_.dissolve(group);
        }
    });
    layout->addWidget(dissolve);
    return line;
}

void LibraryView::pickGroup(const Book& book, QWidget* anchor) {
    QMenu menu(this);
    auto choose = [&](const QString& name, long long groupId) {
        QAction* action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(book.groupId == groupId);
        QObject::connect(action, &QAction::triggered, this, [this, book, groupId] { feature_.assign(book, groupId); });
    };
    choose("No group", 0);
    for (const auto& group : feature_.groups()) choose(Ui::q(group.name), group.id);
    menu.addSeparator();
    QAction* fresh = menu.addAction("New group…");
    QObject::connect(fresh, &QAction::triggered, this, [this, book] {
        bool ok = false;
        QString name = QInputDialog::getText(this, "New group",
            "A series, an author, a course: books in one group are searched together.", QLineEdit::Normal, "", &ok);
        if (!ok) return;
        if (long long groupId = feature_.groupNamed(Ui::s(name))) feature_.assign(book, groupId);
    });
    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void LibraryView::addBook() {
    QString path = QFileDialog::getOpenFileName(this, "Add a book", Ui::q(Paths::browseRoot()), "EPUB books (*.epub)");
    if (path.isEmpty()) return;
    std::string error = feature_.add(Ui::s(path));
    if (!error.empty()) Ui::alert(this, "Couldn’t add the book", error);
}
