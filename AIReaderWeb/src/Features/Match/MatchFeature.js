// The matching game over the words looked up so far: a few cards a round,
// fronts against backs, and another round once they are all paired. Every
// pair put together is recorded, so a card that was missed comes round
// again sooner.
import { dueCards } from "../../Domain/Cards/Card.js";
import { MatchRound } from "../../Domain/Cards/MatchRound.js";
import { Feature } from "../Common/Feature.js";

export const cardsPerRound = 5;

export class MatchFeature extends Feature {
  /** Null until two words have been looked up. */
  round = null;
  roundNumber = 0;
  #env;
  #bookId;
  #writes = Promise.resolve();

  constructor(env, book) {
    super();
    this.#env = env;
    this.#bookId = book?.id ?? 0;
  }

  async nextRound() {
    const due = dueCards(await this.#env.cards.all(this.#bookId), cardsPerRound);
    this.round = due.length < 2 ? null : new MatchRound(due);
    this.roundNumber++;
    this.changed();
  }

  pickFront(card) {
    this.#record(this.round?.pickFront(card));
  }

  pickBack(card) {
    this.#record(this.round?.pickBack(card));
  }

  #record(pick) {
    if (pick) {
      const { cards } = this.round;
      // The two were confused with each other; both need another look.
      const results = pick.matched ? [[cards[pick.front], true]] : [[cards[pick.front], false], [cards[pick.back], false]];
      // One after another, so two quick picks of a card both count.
      for (const [card, correct] of results) this.#writes = this.#writes.then(() => this.#env.cards.record(card.lookupId, correct));
    }
    this.changed();
  }
}
