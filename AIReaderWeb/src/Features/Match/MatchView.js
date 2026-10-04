// Practice: words down one column, the meanings they had — with their
// sentences, the word blanked — down the other. Pick a word, then its meaning.
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";
import { MatchFeature } from "./MatchFeature.js";

export class MatchView extends Screen {
  constructor(env, navigator, book) {
    super(navigator, "Practice");
    this.feature = new MatchFeature(env, book);
    this.watch(this.feature, () => this.#render());
    this.feature.nextRound();
  }

  #card(round, card, side) {
    const selected = side === "front" ? round.selectedFront === card : round.selectedBack === card;
    const missed = round.lastPick && !round.lastPick.matched && round.lastPick[side] === card;
    const { front, back, example } = round.cards[card];
    return h("li", {}, h("button", {
      type: "button",
      class: missed ? "missed" : null,
      "aria-pressed": selected ? "true" : "false",
      disabled: round.isMatched(card),
      onclick: () => (side === "front" ? this.feature.pickFront(card) : this.feature.pickBack(card)),
    }, side === "front" ? h("strong", {}, front) : [h("span", {}, back), example ? h("small", {}, example) : null]));
  }

  #render() {
    const { round } = this.feature;
    if (!round) {
      this.setBody(note("Look up two words or more while reading, then come back to pair them with their meanings."));
      return;
    }
    this.setBody(
      note(round.isComplete ? `All paired${round.misses ? `, ${round.misses} ${round.misses === 1 ? "miss" : "misses"}` : ", no misses"}.` : "Click a word, then the meaning it had."),
      h("div", { class: "match" },
        h("ul", { class: "fronts" }, round.fronts.map((card) => this.#card(round, card, "front"))),
        h("ul", { class: "backs" }, round.backs.map((card) => this.#card(round, card, "back")))),
      round.isComplete ? h("div", { class: "buttons" }, button("Next round", () => this.feature.nextRound())) : null,
    );
  }
}
