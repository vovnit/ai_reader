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
import { articleSections, EntryView } from "./EntryView.js";
import { LookupFeature } from "./LookupFeature.js";

export class LookupView extends Screen {
  /** `link` is the reader's: what may be searched, and how to go to a place. */
  constructor(env, navigator, context, link = null) {
    super(navigator, context.word);
    this.feature = new LookupFeature(env, context, link?.scope ?? {});
    if (canSpeak()) this.addAction("Pronounce", () => speak(context.word, context.language), { title: "Pronounce the word" });
    this.watch(this.feature, () => this.setBody(...lookupBody(env, navigator, this.feature, link)));
    this.feature.start();
  }
}

/** The lookup alone, for a screen that shows it among other things; the ways on open above that screen. */
export function lookupBody(env, navigator, feature, link = null) {
  const { explanation, error, entry, context, scope } = feature;
  if (!explanation) {
    return [articleSections(entry), error ? h("p", { class: "error message" }, error) : note("Looking up…"), sentence(context)];
  }
  const chat = {
    context: wordContext(context.word, context.sentence, explanation),
    hint: "Ask about this word — another example, a nuance, how it differs from a similar one.",
    scope,
  };
  const uncertain = explanation.guessed || explanation.confidence < 0.6;
  return [
    // What the word means here is what the reader came for.
    h("p", { class: "meaning" }, explanation.meaning),
    h("p", { class: "lemma" }, h("strong", {}, explanation.lemma), explanation.formNote ? h("small", {}, explanation.formNote) : null),
    h("div", { class: "buttons" },
      // Only when the dictionary really has the lemma; a guess has no entry.
      entry.length ? button("Dictionary entry", () => navigator.push(new EntryView(navigator, explanation.lemma, entry))) : null,
      scope.corpus ? button("X-ray", () => navigator.push(new XRayView(env, navigator, context.word, link))) : null,
      button("Ask AI", () => navigator.push(new ChatView(env, navigator, chat)))),
    h("hr"),
    sentence(context),
    uncertain ? h("hr") : null,
    uncertain ? note(`${explanation.guessed ? "Догадка, не из словаря\n" : ""}Уверенность: ${Math.round(explanation.confidence * 100)}%`, { class: "note message" }) : null,
  ];
}

function sentence(context) {
  if (!context.sentence) return null;
  return h("div", { class: "sentence" },
    h("p", {}, h("i", {}, context.sentence)),
    canSpeak() ? button("Play sentence", () => speak(context.sentence, context.language), { class: "link" }) : null);
}
