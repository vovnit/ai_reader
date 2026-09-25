#pragma once

#include "Common/Screen.hpp"
#include "Features/Settings/SettingsFeature.hpp"

class QComboBox;
class QLabel;
class QPushButton;

/// Where lookups are sent, where the model may search the web, and where the
/// app's state is shared with the other devices. Saved as they are typed.
class SettingsView : public Screen {
public:
    SettingsView(Env& env, Navigator& navigator);

private:
    Env& env_;
    SettingsFeature feature_;
    QComboBox* model_;
    QPushButton* load_;
    QLabel* status_;
    QPushButton* sync_;
    QLabel* syncStatus_;

    void render();
    /// The model field offers what the endpoint listed, and takes any name.
    void fillModels();
};
