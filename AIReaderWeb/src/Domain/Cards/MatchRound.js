// One round of the matching game: a few cards, their fronts in one column
// and their backs in another, shuffled apart. The reader picks a front and
// a back; a pair that belongs together is put aside, one that does not is
// a miss. Pure state, so it can be checked without a screen.

/** A small seeded generator, so a round can be replayed in a check. */
function generator(seed) {
  let state = seed >>> 0 || 1;
  return () => {
    state ^= state << 13;
    state ^= state >>> 17;
    state ^= state << 5;
    return (state >>> 0) / 0x100000000;
  };
}

function shuffle(items, random) {
  for (let i = items.length - 1; i > 0; i--) {
    const j = Math.floor(random() * (i + 1));
    [items[i], items[j]] = [items[j], items[i]];
  }
}

export class MatchRound {
  /** Indices into `cards`, in the order the fronts and the backs are shown. No back sits in its front's row. */
  fronts;
  backs;
  selectedFront = null;
  selectedBack = null;
  /** The pair most recently put together, `{ front, back, matched }`, until the next pick. */
  lastPick = null;
  misses = 0;
  #matched;

  constructor(cards, seed = Math.floor(Math.random() * 0xffffffff)) {
    this.cards = cards;
    this.#matched = cards.map(() => false);
    const random = generator(seed);
    this.fronts = cards.map((_, index) => index);
    this.backs = [...this.fronts];
    shuffle(this.fronts, random);
    // A row that lines up would give its answer away.
    do shuffle(this.backs, random);
    while (cards.length > 1 && this.fronts.some((front, row) => front === this.backs[row]));
  }

  isMatched(card) {
    return this.#matched[card];
  }

  get matchedCount() {
    return this.#matched.filter(Boolean).length;
  }

  get isComplete() {
    return this.matchedCount === this.cards.length;
  }

  /** A pick of a front or a back. Picking the chosen one again lets go of it; once one of each is chosen the pair resolves and is returned. */
  pickFront(card) {
    return this.#pick("selectedFront", card);
  }

  pickBack(card) {
    return this.#pick("selectedBack", card);
  }

  #pick(side, card) {
    this.lastPick = null;
    if (card < 0 || card >= this.cards.length || this.#matched[card]) return null;
    this[side] = this[side] === card ? null : card;
    if (this.selectedFront === null || this.selectedBack === null) return null;
    const pick = { front: this.selectedFront, back: this.selectedBack, matched: this.selectedFront === this.selectedBack };
    if (pick.matched) this.#matched[pick.front] = true;
    else this.misses++;
    this.selectedFront = null;
    this.selectedBack = null;
    this.lastPick = pick;
    return pick;
  }
}
