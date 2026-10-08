#pragma once

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>

#include <functional>
#include <string>

/// Small helpers the views share, so each screen reads as what it shows.
namespace Ui {

QString q(const std::string& text);
std::string s(const QString& text);
/// Escaped for rich text, line breaks kept.
QString escape(const std::string& text);
/// Rich text, a size down and in the secondary colour.
QString small(const QString& html);

/// A wrapped label for plain text.
QLabel* label(const std::string& text);
/// A wrapped label for rich text.
QLabel* rich(const QString& html);
/// A wrapped label, smaller and quieter: captions, hints, errors.
QLabel* note(const QString& html);
QFrame* separator();

/// A column for a scrolled body, with the usual padding.
QVBoxLayout* column(QWidget* holder, int spacing = 12);
QScrollArea* scrolled(QWidget* content);
/// Takes everything out of a layout. Widgets are deleted once the current
/// event is done, so a row may clear the list it sits in from its own button.
void clear(QLayout* layout);
/// Runs `action` once the current event is done.
void later(std::function<void()> action);

bool confirm(QWidget* parent, const std::string& title, const std::string& text, const std::string& action);
/// Asks which of two actions to take: 1 or 2, or 0 for neither.
int choose(QWidget* parent, const std::string& title, const std::string& text,
           const std::string& first, const std::string& second);
void alert(QWidget* parent, const std::string& title, const std::string& text);

}  // namespace Ui
