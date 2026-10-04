// One screen: a header with the way back, a title and actions, and a body.
// A screen is placed in the main area (the shelf, the reader) or in the
// panel beside it (everything opened from them), which on a narrow window
// covers the main area instead.
import { h } from "./Ui.js";

export class Screen {
  #stops = [];

  constructor(navigator, title, { placement = "panel" } = {}) {
    this.navigator = navigator;
    this.placement = placement;
    this.backButton = h("button", { type: "button", class: "back", onclick: () => navigator.pop(this) }, "‹ Back");
    this.title = h("h1", {}, title);
    this.actions = h("div", { class: "actions" });
    this.body = h("div", { class: "body" });
    this.element = h("section", { class: `screen ${placement}` }, h("header", {}, this.backButton, this.title, this.actions), this.body);
  }

  setTitle(title) {
    this.title.textContent = title;
  }

  addAction(label, action, props = {}) {
    const element = h("button", { type: "button", onclick: action, ...props }, label);
    this.actions.append(element);
    return element;
  }

  setBody(...nodes) {
    this.body.replaceChildren(...nodes.flat().filter(Boolean));
  }

  /** Re-renders on every change of `feature`, until the screen closes. */
  watch(feature, render) {
    this.#stops.push(feature.subscribe(render));
    render();
  }

  /** Runs when `stop` is called at closing: a listener, a timer. */
  onClose(stop) {
    this.#stops.push(stop);
  }

  /** The screen came to the top again, or for the first time. */
  shown() {}

  close() {
    for (const stop of this.#stops) stop();
    this.#stops = [];
  }
}
