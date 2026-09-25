#include "Menu/DisplayView.hpp"

#include "Common/Ui.hpp"
#include "Features/Reader/ReaderFeature.hpp"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

DisplayView::DisplayView(Env& env, Navigator& navigator, ReaderFeature& reader)
    : Screen(navigator, "Display"),
      feature_(env, [&reader](const ReadingStyle& style) { reader.setStyle(style); }) {
    auto* holder = new QWidget;
    QVBoxLayout* column = Ui::column(holder);
    auto* grid = new QGridLayout;
    grid->setColumnStretch(0, 1);
    int rowIndex = 0;

    // A setting as `name  –  value  +`.
    auto row = [&](const QString& name, std::function<void(int)> adjust) {
        grid->addWidget(new QLabel(name), rowIndex, 0);
        auto* minus = new QPushButton("–");
        auto* value = new QLabel;
        value->setAlignment(Qt::AlignCenter);
        value->setMinimumWidth(56);
        auto* plus = new QPushButton("+");
        QObject::connect(minus, &QPushButton::clicked, this, [adjust] { adjust(-1); });
        QObject::connect(plus, &QPushButton::clicked, this, [adjust] { adjust(1); });
        grid->addWidget(minus, rowIndex, 1);
        grid->addWidget(value, rowIndex, 2);
        grid->addWidget(plus, rowIndex, 3);
        ++rowIndex;
        return value;
    };
    scale_ = row("Size", [this](int step) { feature_.adjustScale(step); });
    spacing_ = row("Line spacing", [this](int step) { feature_.adjustLineSpacing(step); });
    margin_ = row("Margins", [this](int step) { feature_.adjustMargin(step); });

    grid->addWidget(new QLabel("Face"), rowIndex, 0);
    face_ = new QComboBox;
    for (const auto& name : ReadingStyle::fontNames()) face_->addItem(Ui::q(name));
    QObject::connect(face_, &QComboBox::activated, this, [this](int index) {
        feature_.setFont(Ui::s(face_->itemText(index)));
    });
    grid->addWidget(face_, rowIndex, 1, 1, 3);
    column->addLayout(grid);

    column->addWidget(Ui::separator());
    preview_ = new QLabel("The quick brown fox jumps over the lazy dog.");
    preview_->setWordWrap(true);
    column->addWidget(preview_);
    setBody(holder);

    feature_.onChange = [this] { render(); };
    render();
}

void DisplayView::render() {
    const ReadingStyle& style = feature_.style();
    scale_->setText(QString::number(style.scale, 'f', 1));
    spacing_->setText(QString::number(style.lineSpacing, 'f', 0));
    margin_->setText(QString::number(style.margin, 'f', 0));
    QString family = Ui::q(style.fontName.empty() ? "Serif" : style.fontName);
    face_->setCurrentText(family);
    QFont font(family);
    font.setPixelSize(qRound(ReadingStyle::basePixelSize * style.scale));
    preview_->setFont(font);
}
