// The stack of open screens. The topmost main screen fills the window; a
// panel screen above it sits beside it, or over it on a narrow window. The
// browser's Back, a phone's back gesture and Escape close the top screen:
// one history entry stands guard while anything is open above the shelf,
// and is put back after each Back while something still is.

export class Navigator {
  #stack = [];
  #main;
  #panel;
  #armed = false;
  #disarming = false;

  constructor(main, panel) {
    this.#main = main;
    this.#panel = panel;
    window.addEventListener("popstate", () => {
      if (this.#disarming) this.#disarming = false;
      else if (this.#stack.length > 1) this.#remove(1);
      this.#armed = false;
      this.#sync();
    });
    window.addEventListener("keydown", (event) => {
      if (event.key !== "Escape" || event.defaultPrevented || document.querySelector("dialog[open]")) return;
      if (this.#stack.length > 1) this.pop();
    });
  }

  get top() {
    return this.#stack.at(-1) ?? null;
  }

  get screens() {
    return [...this.#stack];
  }

  push(screen) {
    this.#stack.push(screen);
    this.#render();
    this.#sync();
  }

  /** Closes the top screen, or `screen` and everything above it. */
  pop(screen = this.top) {
    const index = this.#stack.indexOf(screen);
    if (index > 0) this.#remove(this.#stack.length - index);
    this.#sync();
  }

  /** Closes everything above `screen`. */
  popTo(screen) {
    const index = this.#stack.indexOf(screen);
    if (index >= 0) this.#remove(this.#stack.length - index - 1);
    this.#sync();
  }

  #remove(count) {
    for (const screen of this.#stack.splice(this.#stack.length - count, count)) screen.close();
    this.#render();
  }

  #render() {
    const top = this.top;
    const main = this.#stack.findLast((screen) => screen.placement === "main");
    if (main && this.#main.firstChild !== main.element) this.#main.replaceChildren(main.element);
    const panel = top?.placement === "panel" ? top : null;
    if (panel ? this.#panel.firstChild !== panel.element : this.#panel.firstChild) this.#panel.replaceChildren(...(panel ? [panel.element] : []));
    document.body.classList.toggle("panel-open", !!panel);
    // The first screen in the panel closes it; the ones after go back.
    for (const [index, screen] of this.#stack.entries()) {
      const below = this.#stack[index - 1];
      screen.backButton.hidden = index === 0;
      screen.backButton.textContent = screen.placement === "panel" && below?.placement === "main" ? "Close" : `‹ ${below?.backName ?? "Back"}`;
    }
    top?.shown();
  }

  #sync() {
    if (this.#stack.length > 1 && !this.#armed && !this.#disarming) {
      history.pushState({ aireader: true }, "");
      this.#armed = true;
    } else if (this.#stack.length <= 1 && this.#armed && !this.#disarming) {
      this.#disarming = true;
      this.#armed = false;
      history.back();
    }
  }
}
