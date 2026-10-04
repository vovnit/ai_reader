// A feature holds a screen's state and the actions on it; its view renders
// the state and calls the actions, and is told when the state changed.

export class Feature {
  #listeners = new Set();

  /** Calls `listener` after every change; returns what stops it. */
  subscribe(listener) {
    this.#listeners.add(listener);
    return () => this.#listeners.delete(listener);
  }

  changed() {
    for (const listener of [...this.#listeners]) listener();
  }
}
