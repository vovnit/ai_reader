// What a tapped word means in its sentence: the meaning first, then the
// dictionary form and how the form in the text relates to it, and the ways
// on — the dictionary's own entry, what the book says about the word, a
// conversation about it.
import { wordContext } from "../../Domain/AI/ChatPrompt.js";
import { canSpeak, speak } from "../../Services/Speech.js";
import { ChatView } from "../Chat/ChatView.js";
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";
import { XRayView } from "../XRay/XRayView.js";
import { EntryView } from "./EntryView.js";
import { LookupFeature } from "./LookupFeature.js";

export class LookupView extends Screen {
  /** `link` is the reader's: what may be searched, and how to go to a place. */
  constructor(env, navigator, context, link = null) {
    super(navigator, context.word);
    this.env = env;
    this.link = link;
    this.feature = new LookupFeature(env, context, link?.scope ?? {});
    if (canSpeak()) this.addAction("Pronounce", () => speak(context.word, context.language), { title: "Pronounce the word" });
    this.watch(this.feature, () => this.#render());
    this.feature.start();
  }

  #sentence() {
    const { context } = this.feature;
    if (!context.sentence) return null;
    return h("div", { class: "sentence" },
      h("p", {}, h("i", {}, context.sentence)),
      canSpeak() ? button("Play sentence", () => speak(context.sentence, context.language), { class: "link" }) : null);
  }

  #render() {
    const { explanation, error, entry, context, scope } = this.feature;
    if (!explanation) {
      this.setBody(error ? h("p", { class: "error message" }, error) : note("Looking up…"), this.#sentence());
      return;
    }
    const chat = {
      context: wordContext(context.word, context.sentence, explanation),
      hint: "Ask about this word — another example, a nuance, how it differs from a similar one.",
      scope,
    };
    const uncertain = explanation.guessed || explanation.confidence < 0.6;
    this.setBody(
      // What the word means here is what the reader came for.
      h("p", { class: "meaning" }, explanation.meaning),
      h("p", { class: "lemma" }, h("strong", {}, explanation.lemma), explanation.formNote ? h("small", {}, explanation.formNote) : null),
      h("div", { class: "buttons" },
        // Only when the dictionary really has the lemma; a guess has no entry.
        entry.length ? button("Dictionary entry", () => this.navigator.push(new EntryView(this.navigator, explanation.lemma, entry))) : null,
        scope.corpus ? button("X-ray", () => this.navigator.push(new XRayView(this.env, this.navigator, context.word, this.link))) : null,
        button("Ask AI", () => this.navigator.push(new ChatView(this.env, this.navigator, chat)))),
      h("hr"),
      this.#sentence(),
      uncertain ? h("hr") : null,
      uncertain ? note(`${explanation.guessed ? "Догадка, не из словаря\n" : ""}Уверенность: ${Math.round(explanation.confidence * 100)}%`, { class: "note message" }) : null,
    );
  }
}
