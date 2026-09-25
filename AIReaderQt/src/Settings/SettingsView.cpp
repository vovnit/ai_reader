#include "Settings/SettingsView.hpp"

#include "Common/Navigator.hpp"
#include "Common/Ui.hpp"
#include "Dictionaries/DictionariesView.hpp"
#include "Services/Paths.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

/// A text field that hands every edit to `save`.
QLineEdit* field(QWidget* owner, const std::string& value, std::function<void(const std::string&)> save, bool secret = false) {
    auto* edit = new QLineEdit(Ui::q(value));
    if (secret) edit->setEchoMode(QLineEdit::Password);
    QObject::connect(edit, &QLineEdit::textEdited, owner, [save = std::move(save)](const QString& text) { save(Ui::s(text)); });
    return edit;
}

QFormLayout* section(QVBoxLayout* column, const QString& title) {
    auto* box = new QGroupBox(title);
    auto* form = new QFormLayout(box);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    column->addWidget(box);
    return form;
}

QLabel* hint(const QString& text) {
    return Ui::note(text);
}

}  // namespace

SettingsView::SettingsView(Env& env, Navigator& navigator)
    : Screen(navigator, "Settings", "Done"), env_(env), feature_(env) {
    auto* holder = new QWidget;
    QVBoxLayout* column = Ui::column(holder);
    const AiSettings& ai = feature_.settings();

    QFormLayout* model = section(column, "Model");
    model->addRow("Endpoint", field(this, ai.endpoint, [this](const std::string& v) { feature_.setEndpoint(v); }));
    model->addRow("Token", field(this, ai.apiKey, [this](const std::string& v) { feature_.setToken(v); }, true));
    model->addRow("OpenAI token", field(this, ai.openAIKey, [this](const std::string& v) { feature_.setOpenAIToken(v); }, true));
    model->addRow(hint("Used when the endpoint is api.openai.com. Set the endpoint to <tt>mock://ai</tt> to answer lookups without the network."));
    model_ = new QComboBox;
    model_->setEditable(true);
    QObject::connect(model_, &QComboBox::currentTextChanged, this, [this](const QString& text) { feature_.setModel(Ui::s(text)); });
    load_ = new QPushButton("Load models");
    QObject::connect(load_, &QPushButton::clicked, this, [this] { feature_.loadModels(); });
    model->addRow("Model", model_);
    model->addRow("", load_);
    status_ = Ui::label("");
    model->addRow("", status_);

    auto* dictionaries = new QPushButton("Dictionaries…");
    QObject::connect(dictionaries, &QPushButton::clicked, this, [this] { this->navigator.push(new DictionariesView(env_, this->navigator)); });
    column->addWidget(dictionaries, 0, Qt::AlignLeft);

    const WebSearchSettings& web = feature_.webSearch();
    QFormLayout* search = section(column, "Web search");
    search->addRow(hint("A Monid token lets the model search the web; some endpoints are free, most are paid per call."));
    search->addRow("Monid token", field(this, web.apiKey, [this](const std::string& v) { feature_.setWebToken(v); }, true));
    search->addRow("Provider", field(this, web.provider, [this](const std::string& v) { feature_.setWebProvider(v); }));
    search->addRow("Endpoint", field(this, web.endpoint, [this](const std::string& v) { feature_.setWebEndpoint(v); }));
    search->addRow("Input", field(this, web.input, [this](const std::string& v) { feature_.setWebInput(v); }));
    search->addRow(hint("As <tt>monid inspect</tt> lists it: <tt>$query</tt> is the words searched, <tt>$language</tt> the book’s."));

    const SyncSettings& sync = feature_.sync();
    QFormLayout* share = section(column, "Sync");
    share->addRow(hint("Books, reading places, groups and words are shared with the other devices through a WebDAV folder."));
    share->addRow("WebDAV folder", field(this, sync.url, [this](const std::string& v) { feature_.setSyncUrl(v); }));
    share->addRow("User name", field(this, sync.username, [this](const std::string& v) { feature_.setSyncUser(v); }));
    share->addRow("Password", field(this, sync.password, [this](const std::string& v) { feature_.setSyncPassword(v); }, true));
    sync_ = new QPushButton("Sync now");
    QObject::connect(sync_, &QPushButton::clicked, this, [this] { feature_.syncNow(); });
    share->addRow("", sync_);
    syncStatus_ = Ui::label("");
    share->addRow("", syncStatus_);

    column->addWidget(hint("Settings are kept in " + Ui::escape(Paths::settings())
                           + ", which can also be edited by hand.<br><br>AIReader " AIREADER_VERSION));
    setBody(Ui::scrolled(holder));

    feature_.onChange = [this] { render(); };
    feature_.onModelsChange = [this] { fillModels(); };
    fillModels();
    render();
}

void SettingsView::render() {
    load_->setText(feature_.isLoadingModels() ? "Loading…" : "Load models");
    load_->setEnabled(!feature_.isLoadingModels());
    status_->setText(Ui::q(feature_.error()));
    status_->setVisible(!feature_.error().empty());
    sync_->setText(feature_.isSyncing() ? "Syncing…" : "Sync now");
    sync_->setEnabled(!feature_.isSyncing() && feature_.sync().isConfigured());
    syncStatus_->setText(Ui::q(feature_.syncMessage()));
    syncStatus_->setVisible(!feature_.syncMessage().empty());
}

void SettingsView::fillModels() {
    QSignalBlocker quiet(model_);
    model_->clear();
    for (const auto& name : feature_.models()) model_->addItem(Ui::q(name));
    model_->setCurrentText(Ui::q(feature_.settings().model));
}
