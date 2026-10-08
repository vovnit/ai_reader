#include "Library/LibraryView.hpp"

#include "Common/Navigator.hpp"
#include "Common/Tappable.hpp"
#include "Common/Ui.hpp"
#include "Reader/ReaderView.hpp"
#include "Services/EpubLoader.hpp"
#include "Services/Paths.hpp"
#include "Support/Files.hpp"
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
    feature_.onFailure = [this](const std::string& title, const std::string& message) { Ui::alert(this, title, message); };
    feature_.refresh();
    feature_.sync();
}

void LibraryView::returned() {
    feature_.refresh();
    feature_.sync();
}

void LibraryView::render() {
    Ui::clear(list_);
    if (feature_.books().empty() && feature_.cloudBooks().empty()) {
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
    // Then the books in the sync folder that are not here.
    if (!feature_.cloudBooks().empty()) {
        QLabel* heading = Ui::rich("<b>In the sync folder</b>");
        heading->setContentsMargins(6, 12, 0, 0);
        list_->addWidget(heading);
    }
    for (const auto& name : feature_.cloudBooks()) {
        list_->addWidget(cloudRow(name));
        list_->addWidget(Ui::separator());
    }
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
    QObject::connect(remove, &QPushButton::clicked, this, [this, book] { this->remove(book); });
    layout->addWidget(group);
    layout->addWidget(remove);
    return line;
}

void LibraryView::remove(const Book& book) {
    if (!feature_.isInCloud(book)) {
        if (Ui::confirm(this, "Remove “" + book.title + "”?",
                        "The book file is deleted. Words looked up in it are kept.", "Remove")) {
            feature_.remove(book);
        }
        return;
    }
    int choice = Ui::choose(this, "Remove “" + book.title + "”?",
                            "Its copy in the sync folder can stay for your other devices, or go too. "
                            "Devices that already have it keep theirs. Words looked up in it are kept.",
                            "From this device", "From the sync folder too");
    if (choice == 1) feature_.remove(book);
    if (choice == 2) feature_.removeEverywhere(book);
}

QWidget* LibraryView::cloudRow(const std::string& name) {
    auto* line = new QWidget;
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(6, 6, 0, 6);
    std::string title = Files::stem(name);
    bool downloading = feature_.isDownloading(name);
    layout->addWidget(Ui::rich(Ui::escape(title) + (downloading ? "<br>" + Ui::small("Downloading…") : QString())), 1);

    auto* download = new QPushButton("Download");
    download->setEnabled(!downloading);
    QObject::connect(download, &QPushButton::clicked, this, [this, name] { feature_.download(name); });
    auto* remove = new QPushButton("✕");
    remove->setToolTip("Delete it from the sync folder");
    QObject::connect(remove, &QPushButton::clicked, this, [this, name, title] {
        if (Ui::confirm(this, "Delete “" + title + "” from the sync folder?",
                        "Devices that already have it keep their copy.", "Delete")) {
            feature_.removeRemote(name);
        }
    });
    layout->addWidget(download);
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
    QString path = QFileDialog::getOpenFileName(this, "Add a book", Ui::q(Paths::browseRoot()), "Books (*.epub *.pdf)");
    if (path.isEmpty()) return;
    std::string error = feature_.add(Ui::s(path));
    if (!error.empty()) Ui::alert(this, "Couldn’t add the book", error);
}
