// A name, a place, a word the book uses its own way: what the book has said
// about it so far, and the passages it said it in, each a click from its place.
import { xrayContext } from "../../Domain/AI/ChatPrompt.js";
import { ChatView } from "../Chat/ChatView.js";
import { Screen } from "../Common/Screen.js";
import { button, h, marked, note } from "../Common/Ui.js";
import { XRayFeature } from "./XRayFeature.js";

export class XRayView extends Screen {
  /** With a `term` it asks at once; without, it asks the reader for one. */
  constructor(env, navigator, term, link) {
    super(navigator, term || "X-ray");
    this.env = env;
    this.link = link;
    this.feature = new XRayFeature(env, link?.scope ?? {});
    this.input = h("input", { type: "search", placeholder: "A name, a place, a word", value: term, enterkeyhint: "search" });
    this.form = h("form", { class: "ask", onsubmit: (event) => {
      event.preventDefault();
      this.setTitle(this.input.value.trim() || "X-ray");
      this.feature.ask(this.input.value);
    } }, this.input, h("button", {}, "X-ray"));
    this.watch(this.feature, () => this.#render());
    if (term) this.feature.ask(term);
  }

  shown() {
    if (!this.feature.term) this.input.focus();
  }

  #render() {
    const { term, passages, answer, error, isWorking } = this.feature;
    const several = this.link?.scope?.corpus?.severalBooks;
    const chat = { context: xrayContext(term, answer), hint: "Ask about this — who they are to someone else, where it was first mentioned, what it stands for.", scope: this.feature.scope };
    this.setBody(
      term ? null : [this.form, note("A name, a place, a word the book uses its own way: what the book has said about it so far.")],
      isWorking ? note("Reading the book…") : null,
      error ? h("p", { class: "error message" }, error) : null,
      answer ? h("p", { class: "answer message" }, answer) : null,
      answer ? h("div", { class: "buttons" }, button("Ask AI", () => this.navigator.push(new ChatView(this.env, this.navigator, chat)))) : null,
      term && !isWorking ? h("h2", {}, passages.length ? "Where it has appeared so far" : "Not met yet in what has been read.") : null,
      h("ul", { class: "hits" }, passages.map((hit) => h("li", {}, h("button", { type: "button", onclick: () => this.link.goTo(hit) },
        h("small", {}, `${several ? `${hit.bookTitle} · ` : ""}Chapter ${hit.chapter + 1}`),
        h("span", {}, marked(hit.excerpt, hit.matchStart, hit.matchEnd)))))),
    );
  }
}
