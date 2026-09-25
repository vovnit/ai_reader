#pragma once

#include "Common/Screen.hpp"
#include "Features/Dictionaries/DictionariesFeature.hpp"

#include <functional>

class QPushButton;
class QVBoxLayout;

/// The dictionaries lookups search: the bundled one and any added, each of
/// them switched on or off.
class DictionariesView : public Screen {
public:
    DictionariesView(Env& env, Navigator& navigator);

private:
    DictionariesFeature feature_;
    QVBoxLayout* list_;
    QPushButton* scan_;
    QPushButton* add_;

    void render();
    QWidget* row(const DictionaryPack& pack);
    void addFile();
    /// Converting a big dictionary takes a while and holds the window; the
    /// button says so before the work starts.
    void busy(QPushButton* button, const std::function<std::string()>& work);
};
