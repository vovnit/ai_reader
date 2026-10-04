// The conversation: questions and answers in turn, the model's answers set
// from their Markdown, and a field for the next question.
import { markdownLines } from "../../Domain/AI/ChatMarkdown.js";
import { Screen } from "../Common/Screen.js";
import { h, note } from "../Common/Ui.js";
import { ChatFeature } from "./ChatFeature.js";

function runs(line) {
  return line.runs.map((run) => {
    let node = run.href && /^https?:/i.test(run.href) ? h("a", { href: run.href, target: "_blank", rel: "noopener" }, run.text) : run.text;
    if (run.code) node = h("code", {}, node);
    if (run.italic) node = h("em", {}, node);
    if (run.bold) node = h("strong", {}, node);
    return node;
  });
}

function answer(text) {
  return h("div", { class: "turn model" }, markdownLines(text).map((line) => line.kind === "rule"
    ? h("hr")
    : h(line.kind === "code" ? "pre" : "p", { class: line.kind }, line.prefix, runs(line))));
}

export class ChatView extends Screen {
  /** `seed` is `{ context, hint, scope }`: what the conversation is about. */
  constructor(env, navigator, seed) {
    super(navigator, "Chat");
    this.feature = new ChatFeature(env, seed.context, seed.scope);
    this.input = h("textarea", { rows: 2, placeholder: "Ask a question", enterkeyhint: "send", onkeydown: (event) => {
      if (event.key === "Enter" && !event.shiftKey) {
        event.preventDefault();
        this.#send();
      }
    } });
    this.sendButton = h("button", {}, "Send");
    this.form = h("form", { class: "ask", onsubmit: (event) => {
      event.preventDefault();
      this.#send();
    } }, this.input, this.sendButton);
    this.hint = seed.hint;
    this.log = h("div", { class: "turns", "aria-live": "polite" });
    this.body.classList.add("chat");
    this.setBody(this.log, this.form);
    this.watch(this.feature, () => this.#render());
  }

  shown() {
    this.input.focus();
  }

  #send() {
    const question = this.input.value;
    if (!question.trim() || this.feature.isAnswering) return;
    this.input.value = "";
    this.feature.send(question);
  }

  #render() {
    const { turns, isAnswering, error } = this.feature;
    this.log.replaceChildren(...[
      turns.length ? null : note(this.hint),
      ...turns.map((turn) => (turn.isReader ? h("p", { class: "turn reader" }, turn.text) : answer(turn.text))),
      isAnswering ? note("…") : null,
      error ? h("p", { class: "error message" }, error) : null,
    ].filter(Boolean));
    this.sendButton.disabled = isAnswering;
    this.log.lastElementChild?.scrollIntoView({ block: "end" });
  }
}
