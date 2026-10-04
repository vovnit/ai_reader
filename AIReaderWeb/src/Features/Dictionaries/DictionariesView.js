// The dictionaries, each one switched on or off; added ones can be removed.
import { formatLabels } from "../../Domain/Formats/DictionaryFormat.js";
import { packLanguages } from "../../Services/DictionaryPacks.js";
import { confirmAction } from "../Common/Dialogs.js";
import { pickFiles } from "../Common/Files.js";
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";
import { DictionariesFeature } from "./DictionariesFeature.js";

// The bundled dictionary's sources ask to be named, and their license with them.
const credits = "The dictionary that ships with the app is selected and converted from Lexique 4, by Boris New, "
  + "Christophe Pallier and others (lexique.org), and from the Russian Wiktionary, by its contributors "
  + "(ru.wiktionary.org, by way of kaikki.org). Both are licensed CC BY-SA 4.0 "
  + "(creativecommons.org/licenses/by-sa/4.0), and so is this dictionary.";

const accept = ".sqlite3,.sqlite,.db,.tsv,.csv,.txt,.dsl,.dz,.gz,.ifo,.idx,.dict,.xdxf,.xml";

export class DictionariesView extends Screen {
  constructor(env, navigator) {
    super(navigator, "Dictionaries");
    this.feature = new DictionariesFeature(env);
    this.addAction("Add…", async () => {
      const files = await pickFiles({ accept });
      if (files.length) await this.feature.add(files);
    });
    this.watch(this.feature, () => this.#render());
    this.feature.reload();
  }

  async #remove(pack) {
    if (await confirmAction({ title: `Remove “${pack.name}”?`, confirm: "Remove" })) await this.feature.remove(pack);
  }

  #render() {
    const { packs, isAdding, message } = this.feature;
    this.setBody(
      h("ul", { class: "packs" }, packs.map((pack) => h("li", {},
        h("label", {},
          h("input", { type: "checkbox", checked: pack.isEnabled, onchange: () => this.feature.toggle(pack) }),
          h("span", {}, h("strong", {}, pack.name), h("small", {}, [packLanguages(pack), pack.kind === "bundled" ? "Ships with the app" : pack.fileName].filter(Boolean).join(" · ")))),
        pack.kind === "bundled" ? null : button("Remove", () => this.#remove(pack), { class: "link" })))),
      isAdding ? note("Adding… a large dictionary takes a while.") : null,
      message ? h("p", { class: "message" }, message) : null,
      note(`Add a dictionary: a pack (.sqlite3), a word list (.tsv, .csv: one word and its meaning per line), `
        + `or ${[formatLabels.dsl, formatLabels.xdxf].join(", ")} and ${formatLabels.stardict} — pick the .ifo, .idx and .dict together. `
        + "Every dictionary switched on is searched, and their answers merged."),
      note(credits),
    );
  }
}
