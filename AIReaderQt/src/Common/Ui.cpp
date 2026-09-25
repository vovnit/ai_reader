#include "Common/Ui.hpp"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace Ui {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

std::string s(const QString& text) {
    return text.toStdString();
}

QString escape(const std::string& text) {
    return q(text).toHtmlEscaped().replace('\n', "<br>");
}

QString small(const QString& html) {
    // Rich text knows no palette roles; the colour is looked up now.
    QString colour = QApplication::palette().color(QPalette::PlaceholderText).name();
    return "<small><span style=\"color: " + colour + "\">" + html + "</span></small>";
}

QLabel* label(const std::string& text) {
    auto* label = new QLabel(q(text));
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QLabel* rich(const QString& html) {
    auto* label = new QLabel(html);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    return label;
}

QLabel* note(const QString& html) {
    return rich(small(html));
}

QFrame* separator() {
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

QVBoxLayout* column(QWidget* holder, int spacing) {
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(spacing);
    layout->setAlignment(Qt::AlignTop);
    return layout;
}

QScrollArea* scrolled(QWidget* content) {
    auto* area = new QScrollArea;
    area->setWidget(content);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return area;
}

void clear(QLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
            delete item;
        } else if (QLayout* inner = item->layout()) {
            // A nested layout is its own item.
            clear(inner);
            delete inner;
        } else {
            delete item;
        }
    }
}

void later(std::function<void()> action) {
    QTimer::singleShot(0, [action = std::move(action)] { action(); });
}

bool confirm(QWidget* parent, const std::string& title, const std::string& text, const std::string& action) {
    QMessageBox box(QMessageBox::Question, "AIReader", q(title), QMessageBox::Cancel, parent);
    box.setInformativeText(q(text));
    QPushButton* go = box.addButton(q(action), QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() == go;
}

void alert(QWidget* parent, const std::string& title, const std::string& text) {
    QMessageBox box(QMessageBox::Information, "AIReader", q(title), QMessageBox::Ok, parent);
    box.setInformativeText(q(text));
    box.exec();
}

}  // namespace Ui
