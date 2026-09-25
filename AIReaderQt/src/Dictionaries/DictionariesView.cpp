#include "Dictionaries/DictionariesView.hpp"

#include "Common/Ui.hpp"
#include "Domain/Formats/DictionaryFormat.hpp"
#include "Services/Paths.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

DictionariesView::DictionariesView(Env& env, Navigator& navigator)
    : Screen(navigator, "Dictionaries"), feature_(env) {
    auto* holder = new QWidget;
    list_ = Ui::column(holder);
    setBody(Ui::scrolled(holder));
    scan_ = addAction("Scan folder", [this] { busy(scan_, [this] { return feature_.addFromFolder(); }); });
    add_ = addAction("Add…", [this] { addFile(); });
    feature_.onChange = [this] { render(); };
    render();
}

void DictionariesView::render() {
    Ui::clear(list_);
    for (const auto& pack : feature_.packs()) list_->addWidget(row(pack));
    list_->addWidget(Ui::separator());
    list_->addWidget(Ui::note(
        "Add a dictionary from anywhere: a pack (<tt>.sqlite3</tt>), a word list (<tt>.tsv</tt>, <tt>.csv</tt>: one "
        "headword and its meaning per line), Lingvo DSL, StarDict or XDXF. Or copy files into<br><tt>"
        + Ui::escape(Paths::dictionaries()) + "</tt><br>and choose Scan folder."));
}

QWidget* DictionariesView::row(const DictionaryPack& pack) {
    auto* line = new QWidget;
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* enabled = new QCheckBox;
    enabled->setChecked(pack.isEnabled);
    QObject::connect(enabled, &QCheckBox::toggled, this, [this, pack] { feature_.toggle(pack); });
    layout->addWidget(enabled);

    QString text = "<b>" + Ui::escape(pack.name) + "</b>";
    std::string detail = pack.languages();
    if (pack.isBundled()) detail += detail.empty() ? "Ships with the app" : " · ships with the app";
    if (!detail.empty()) text += "<br>" + Ui::small(Ui::escape(detail));
    layout->addWidget(Ui::rich(text), 1);

    if (!pack.isBundled()) {
        auto* remove = new QPushButton("✕");
        remove->setToolTip("Remove the dictionary");
        QObject::connect(remove, &QPushButton::clicked, this, [this, pack] { feature_.remove(pack); });
        layout->addWidget(remove);
    }
    return line;
}

void DictionariesView::addFile() {
    QString path = QFileDialog::getOpenFileName(this, "Add a dictionary", Ui::q(Paths::browseRoot()),
        "Dictionaries (*.sqlite3 *.sqlite *.db *.tsv *.csv *.txt *.dsl *.dz *.ifo *.xdxf *.xml);;All files (*)");
    if (path.isEmpty()) return;
    std::string file = Ui::s(path);
    if (!DictionaryFormats::of(file)) {
        Ui::alert(this, "Dictionaries", "That is not a dictionary this app can read.");
        return;
    }
    busy(add_, [this, file] { return feature_.addFile(file); });
}

void DictionariesView::busy(QPushButton* button, const std::function<std::string()>& work) {
    QString was = button->text();
    button->setText("Adding…");
    button->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();
    std::string message = work();
    QApplication::restoreOverrideCursor();
    button->setText(was);
    button->setEnabled(true);
    Ui::alert(this, "Dictionaries", message);
}
