// A book's offline glossary: how many of its words have a definition, what
// defining the rest should cost, and the button that does it.
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";
import { GlossaryFeature } from "./GlossaryFeature.js";

const number = (value) => value.toLocaleString();

export class GlossaryView extends Screen {
  /** `chapters` are the book's texts, `language` its language. */
  constructor(env, navigator, book, chapters, language) {
    super(navigator, "Offline glossary");
    this.feature = new GlossaryFeature(env, book, chapters, language);
    this.watch(this.feature, () => this.#render());
    this.feature.count();
  }

  #render() {
    const { isCounted, total, defined, isRunning, error, estimatedTokens, name, model } = this.feature;
    const about = h("p", {}, "The model is asked, once, what each word of this book means where it stands. "
      + `Its answers become the dictionary “${name}”, so lookups in this book work without a network.`);
    if (!isCounted) {
      this.setBody(about, note("Counting words…"));
      return;
    }
    const left = total - defined;
    let status;
    if (isRunning) status = `Defined ${number(defined)} of ${number(total)} words.`;
    else if (!left) status = `All ${number(total)} words are defined.`;
    else {
      const done = defined ? `${number(defined)} of ${number(total)} words are defined; the rest` : `${number(total)} words`;
      status = `${done} should take about ${number(estimatedTokens)} tokens with ${model}.`;
    }
    this.setBody(
      about,
      h("p", {}, status),
      error ? h("p", { class: "error message" }, error) : null,
      isRunning ? button("Stop", () => this.feature.stop())
        : left ? button(defined ? "Continue" : "Write glossary", () => this.feature.start()) : null,
    );
  }
}
