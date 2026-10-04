// The settings form. Fields are built once and saved as they are typed;
// only the parts that answer back — the model list, sync — are redrawn.
import { DictionariesView } from "../Dictionaries/DictionariesView.js";
import { Screen } from "../Common/Screen.js";
import { button, field, h, note } from "../Common/Ui.js";
import { SettingsFeature } from "./SettingsFeature.js";

const syncNote = "Books go to its Books folder, and reading places, groups and looked-up words to one file beside it, "
  + "shared with the iOS, Kindle and Linux apps. Syncs when the app opens and when a book is closed. "
  + "The server must accept requests from this page (CORS) — for rclone, start it with --allow-origin.";

export class SettingsView extends Screen {
  constructor(env, navigator) {
    super(navigator, "Settings");
    const feature = (this.feature = new SettingsFeature(env));
    const input = (value, onInput, props = {}) => h("input", { value, oninput: (event) => onInput(event.target.value), autocomplete: "off", spellcheck: "false", ...props });
    const ai = (name, props) => input(feature.ai[name], (value) => feature.setAi(name, value), props);
    const web = (name, props) => input(feature.web[name], (value) => feature.setWeb(name, value), props);
    const sync = (name, props) => input(feature.sync[name], (value) => feature.setSync(name, value), props);

    this.model = h("div");
    this.loadButton = button("Load models", () => feature.loadModels());
    this.error = h("div");
    this.syncButton = button("Sync now", () => feature.syncNow());
    this.syncMessage = h("small", { class: "message" });
    this.setBody(h("form", { class: "settings", onsubmit: (event) => event.preventDefault() },
      h("fieldset", {}, h("legend", {}, "Service"),
        field("Endpoint", ai("endpoint", { type: "url", inputmode: "url" })),
        field("Token", ai("apiKey", { type: "password" })),
        field("OpenAI token", ai("openAIKey", { type: "password" }), "Used when the endpoint is api.openai.com. Set the endpoint to mock://ai to answer lookups without the network.")),
      h("fieldset", {}, h("legend", {}, "Model"), this.model, h("div", { class: "buttons" }, this.loadButton), this.error),
      h("fieldset", {}, h("legend", {}, "Explain in"),
        field("Language", ai("language"), "The language of explanations and answers, such as English.")),
      h("div", { class: "buttons" }, button("Dictionaries…", () => navigator.push(new DictionariesView(env, navigator)))),
      h("fieldset", {}, h("legend", {}, "Web search"),
        field("Monid token", web("apiKey", { type: "password" })),
        field("Provider", web("provider")),
        field("Endpoint", web("endpoint")),
        field("Input", h("textarea", { rows: 3, spellcheck: "false", value: feature.web.input, oninput: (event) => feature.setWeb("input", event.target.value) }),
          "With a Monid token the model can search the web for a name, a place or an expression the dictionary and the book do not explain. "
          + "The provider and endpoint are as “monid discover” lists them; the input is what the endpoint is sent, with $query for the words searched "
          + "and $language for the book's language. Some endpoints are free, most are paid per call.")),
      h("fieldset", {}, h("legend", {}, "Sync"),
        field("WebDAV folder", sync("url", { type: "url", inputmode: "url" })),
        field("User name", sync("username", { autocomplete: "username" })),
        field("Password", sync("password", { type: "password", autocomplete: "current-password" })),
        h("div", { class: "buttons" }, this.syncButton), this.syncMessage),
      note("Settings, tokens and the password are kept in this browser's storage, which it does not encrypt.")));
    this.watch(feature, () => this.#update());
  }

  #update() {
    const { ai, models, isLoadingModels, error, isSyncing, syncMessage, canSync } = this.feature;
    const current = this.model.querySelector("select, input");
    const wanted = models.length ? "SELECT" : "INPUT";
    // Typing into the field must not rebuild it under the cursor.
    if (current?.tagName !== wanted || (wanted === "SELECT" && current.options.length !== models.length)) {
      const control = models.length
        ? h("select", { onchange: (event) => this.feature.setAi("model", event.target.value) }, models.map((name) => h("option", { value: name }, name)))
        : h("input", { value: ai.model, autocomplete: "off", spellcheck: "false", oninput: (event) => this.feature.setAi("model", event.target.value) });
      this.model.replaceChildren(field("Model", control));
    }
    if (models.length) this.model.querySelector("select").value = ai.model;
    this.loadButton.textContent = isLoadingModels ? "Loading…" : "Load models";
    this.loadButton.disabled = isLoadingModels;
    this.error.replaceChildren(...(error ? [h("p", { class: "error message" }, error)] : []));
    this.syncButton.textContent = isSyncing ? "Syncing…" : "Sync now";
    this.syncButton.disabled = !canSync;
    this.syncMessage.textContent = syncMessage || syncNote;
  }
}
