// Flash cards, the matching game and the Anki file — the Kindle check's cases.
import { ankiText } from "../../src/Domain/Cards/AnkiExport.js";
import { cardFromLookup, dueCards } from "../../src/Domain/Cards/Card.js";
import { MatchRound } from "../../src/Domain/Cards/MatchRound.js";
import { check } from "./Check.mjs";

export function checkCards() {
  const lookup = { id: 7, word: "a", sentence: "Il a vu. Elle avait a peine parlé, a-t-il dit.", lemma: "avoir", meaning: "иметь" };
  const card = cardFromLookup(lookup);
  check("card blanks whole words only", card.example === "Il ____ vu. Elle avait ____ peine parlé, ____-t-il dit.", card.example);
  check("card carries the lemma when it differs", card.front === "a" && card.lemma === "avoir" && card.back === "иметь" && card.lookupId === 7);
  const same = cardFromLookup({ ...lookup, word: "été", lemma: "été", sentence: "L’été fut chaud." });
  check("card blanks after an apostrophe and hides a same lemma", same.example === "L’____ fut chaud." && same.lemma === "", same.example);
  const plain = cardFromLookup({ id: 8, word: "chat", lemma: "chat", sentence: "Le chat & le <chien>.", meaning: "кот" });
  const anki = ankiText([card, plain], "AIReader::Le\tLivre");
  check("anki export: headers, a note per line, fields a tab apart, html escaped", anki
    === "#separator:tab\n#html:true\n#deck:AIReader::Le Livre\n"
    + "<b>a</b> (avoir)<br><i>Il a vu. Elle avait a peine parlé, a-t-il dit.</i>\tиметь\n"
    + "<b>chat</b><br><i>Le chat &amp; le &lt;chien&gt;.</i>\tкот\n", anki);

  const fresh = { lookupId: 1, correct: 0, wrong: 0, practicedAt: "" };
  const missed = { lookupId: 2, correct: 0, wrong: 2, practicedAt: "2026-09-13T00:00:00Z" };
  const seen = { lookupId: 3, correct: 3, wrong: 0, practicedAt: "2026-09-12T00:00:00Z" };
  const old = { lookupId: 4, correct: 3, wrong: 0, practicedAt: "2026-09-01T00:00:00Z" };
  const due = dueCards([seen, old, missed, fresh], 3).map((dueCard) => dueCard.lookupId);
  check("due cards: never practised, then missed, then longest unseen", due.join() === "1,2,4", due.join());

  const cards = Array.from({ length: 5 }, (_, i) => ({ lookupId: i + 1, front: `w${i}`, back: `m${i}` }));
  let everShuffled = false;
  let everAligned = false;
  let everBroken = false;
  for (let seed = 1; seed <= 20; seed++) {
    const round = new MatchRound(cards, seed);
    const sorted = (list) => [...list].sort().join();
    if (sorted(round.fronts) !== "0,1,2,3,4" || sorted(round.backs) !== "0,1,2,3,4") everBroken = true;
    if (round.fronts.some((front, row) => front === round.backs[row])) everAligned = true;
    if (round.fronts.join() !== "0,1,2,3,4") everShuffled = true;
  }
  check("round shows every card once on each side", !everBroken);
  check("round shuffles and never lines a pair up", everShuffled && !everAligned);
  const round = new MatchRound(cards, 3);
  check("a lone pick resolves nothing", !round.pickFront(2) && round.selectedFront === 2 && !round.lastPick);
  check("picking the chosen card lets go of it", !round.pickFront(2) && round.selectedFront === null);
  round.pickFront(2);
  const miss = round.pickBack(4);
  check("a wrong back is a miss", miss && !miss.matched && miss.front === 2 && miss.back === 4 && round.misses === 1 && !round.isMatched(2));
  check("a miss clears the choice", round.selectedFront === null && round.selectedBack === null && round.lastPick && !round.lastPick.matched);
  round.pickBack(2);
  const hit = round.pickFront(2);
  check("back then front pairs too", hit?.matched && round.isMatched(2) && round.matchedCount === 1);
  check("a paired card takes no more picks", !round.pickFront(2) && round.selectedFront === null);
  for (const i of [0, 1, 3, 4]) {
    round.pickFront(i);
    round.pickBack(i);
  }
  check("round completes", round.isComplete && round.matchedCount === 5 && round.misses === 1);
  const pair = new MatchRound(cards.slice(0, 2), 9);
  check("two cards make a round with swapped rows", pair.fronts[0] !== pair.backs[0] && pair.fronts[1] !== pair.backs[1]);
}
