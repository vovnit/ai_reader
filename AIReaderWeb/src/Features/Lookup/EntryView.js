// The article a lookup was drawn from, as the dictionary has it: every
// sense, and which dictionary it came from.
import { Screen } from "../Common/Screen.js";
import { h } from "../Common/Ui.js";

export class EntryView extends Screen {
  constructor(navigator, lemma, articles) {
    super(navigator, lemma);
    this.setBody(articles.map((article) => h("section", { class: "article" },
      h("h2", {}, article.lemma, article.partOfSpeech ? h("small", {}, ` ${article.partOfSpeech}`) : null),
      h("ol", {}, article.senses.map((sense) => h("li", {}, sense))),
      article.source ? h("p", { class: "note" }, article.source) : null)));
  }
}
