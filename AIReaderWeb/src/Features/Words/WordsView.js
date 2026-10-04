// The words met, each with the meaning it had and the sentence it came
// from; the practice game and the export for Anki above them.
import { confirmAction } from "../Common/Dialogs.js";
import { saveFile } from "../Common/Files.js";
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";
import { LookupView } from "../Lookup/LookupView.js";
import { MatchView } from "../Match/MatchView.js";
import { WordsFeature } from "./WordsFeature.js";

export class WordsView extends Screen {
  constructor(env, navigator, book) {
    super(navigator, book ? "Lookups" : "Words");
    this.feature = new WordsFeature(env, book);
    this.practice = this.addAction("Practice", () => navigator.push(new MatchView(env, navigator, book)));
    this.export = this.addAction("Export for Anki", () => {
      const { name, text } = this.feature.ankiFile();
      saveFile(name, new Blob([text], { type: "text/plain" }));
    });
    this.open = (lookup) => navigator.push(new LookupView(env, navigator, { word: lookup.word, sentence: lookup.sentence, language: lookup.language ?? "", bookId: lookup.bookId }));
    this.watch(this.feature, () => this.#render());
  }

  shown() {
    this.feature.reload();
  }

  async #forget(lookup) {
    const ok = await confirmAction({ title: `Forget “${lookup.word}”?`, message: "It goes from the list and from practice, on your other devices too.", confirm: "Forget" });
    if (ok) await this.feature.remove(lookup);
  }

  #render() {
    const { lookups } = this.feature;
    this.practice.disabled = lookups.length < 2;
    this.export.disabled = !lookups.length;
    this.setBody(lookups.length
      ? h("ul", { class: "words" }, lookups.map((lookup) => h("li", {},
        h("button", { type: "button", class: "open", onclick: () => this.open(lookup) },
          h("strong", {}, lookup.word),
          lookup.lemma && lookup.lemma !== lookup.word ? h("small", {}, ` ${lookup.lemma}`) : null,
          h("span", {}, lookup.meaning),
          h("i", {}, lookup.sentence)),
        button("Forget", () => this.#forget(lookup), { class: "link" }))))
      : note("No words yet. Words you look up while reading collect here."));
  }
}
