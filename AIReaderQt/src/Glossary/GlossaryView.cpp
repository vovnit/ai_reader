#include "Glossary/GlossaryView.hpp"

#include "Common/Ui.hpp"
#include "Features/Reader/ReaderFeature.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

GlossaryView::GlossaryView(Env& env, Navigator& navigator, ReaderFeature& reader)
    : Screen(navigator, "Offline glossary"),
      feature_(env, reader.book(), reader.chapterTexts(), reader.language()) {
    auto* holder = new QWidget;
    column_ = Ui::column(holder, 14);
    setBody(Ui::scrolled(holder));
    feature_.onChange = [this] { render(); };
    render();
    feature_.count();
}

void GlossaryView::render() {
    Ui::clear(column_);
    column_->addWidget(Ui::label("The model is asked, once, what each word of this book means where it stands. "
                                 "Its answers become the dictionary “" + feature_.name() + "”, so lookups in this book "
                                 "work without a network."));
    if (!feature_.isCounted()) {
        column_->addWidget(Ui::label("Counting words…"));
        return;
    }
    column_->addWidget(Ui::label(feature_.status()));
    if (!feature_.error().empty()) column_->addWidget(Ui::label(feature_.error()));

    QPushButton* button = nullptr;
    if (feature_.isRunning()) {
        button = new QPushButton("Stop");
        QObject::connect(button, &QPushButton::clicked, this, [this] { feature_.stop(); });
    } else if (feature_.defined() < feature_.total()) {
        button = new QPushButton(feature_.defined() > 0 ? "Continue" : "Write glossary");
        QObject::connect(button, &QPushButton::clicked, this, [this] { feature_.start(); });
    }
    if (button) {
        auto* line = new QHBoxLayout;
        line->addWidget(button);
        line->addStretch();
        column_->addLayout(line);
    }
}
