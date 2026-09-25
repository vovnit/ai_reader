#include "Common/Screen.hpp"

#include "Common/Navigator.hpp"
#include "Common/Ui.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

Screen::Screen(Navigator& navigator, const QString& title, const QString& back) : navigator(navigator) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);

    header_ = new QHBoxLayout;
    header_->setContentsMargins(8, 6, 8, 6);
    header_->setSpacing(6);
    if (!back.isEmpty()) {
        auto* button = new QPushButton("‹ " + back);
        button->setFlat(true);
        QObject::connect(button, &QPushButton::clicked, this, [&navigator] { navigator.pop(); });
        header_->addWidget(button);
    }
    title_ = new QLabel;
    QFont font = title_->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.25);
    title_->setFont(font);
    title_->setTextFormat(Qt::PlainText);
    header_->addWidget(title_, 1);
    setTitle(title);

    layout_->addLayout(header_);
    layout_->addWidget(Ui::separator());
}

void Screen::setBody(QWidget* body) {
    layout_->addWidget(body, 1);
}

void Screen::setTitle(const QString& title) {
    title_->setText(title);
}

QPushButton* Screen::addAction(const QString& label, std::function<void()> action) {
    auto* button = new QPushButton(label);
    QObject::connect(button, &QPushButton::clicked, this, [action = std::move(action)] { action(); });
    header_->addWidget(button);
    return button;
}
