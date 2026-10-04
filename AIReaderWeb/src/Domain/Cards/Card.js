// A flash card made from a lookup, the way Anki holds one: the word on the
// front, what it meant there on the back, and the sentence it was met in
// with the word blanked out. The practice record says how it has fared:
// `{ lookupId, front, lemma, back, sentence, example, correct, wrong, practicedAt }`.
import { isLetterOrDigit } from "../../Support/Text.js";

const gap = "____";

/** `sentence` with every whole-word occurrence of `word` blanked. */
export function blank(sentence, word) {
  if (!word) return sentence;
  let out = sentence;
  for (let at = out.indexOf(word); at >= 0; at = out.indexOf(word, at)) {
    // "a" inside "avait" is not the word.
    const before = at > 0 ? String.fromCodePoint(out.codePointAt(at - 1 - (isLow(out, at - 1) ? 1 : 0))) : "";
    const after = at + word.length < out.length ? String.fromCodePoint(out.codePointAt(at + word.length)) : "";
    if ((before && isLetterOrDigit(before)) || (after && isLetterOrDigit(after))) {
      at++;
      continue;
    }
    out = out.slice(0, at) + gap + out.slice(at + word.length);
    at += gap.length;
  }
  return out;
}

function isLow(text, i) {
  const code = text.charCodeAt(i);
  return code >= 0xdc00 && code <= 0xdfff && i > 0;
}

export function cardFromLookup(lookup) {
  return {
    lookupId: lookup.id,
    front: lookup.word,
    lemma: lookup.lemma !== lookup.word ? lookup.lemma : "",
    back: lookup.meaning,
    sentence: lookup.sentence,
    example: blank(lookup.sentence, lookup.word),
    correct: 0,
    wrong: 0,
    practicedAt: "",
  };
}

/** Up to `count` cards to practise next: the never-practised first, then the most missed, then the longest unseen. */
export function dueCards(cards, count) {
  return [...cards]
    .sort((a, b) => {
      if (!a.practicedAt !== !b.practicedAt) return a.practicedAt ? 1 : -1;
      const missedA = a.wrong - a.correct;
      const missedB = b.wrong - b.correct;
      if (missedA !== missedB) return missedB - missedA;
      return a.practicedAt < b.practicedAt ? -1 : a.practicedAt > b.practicedAt ? 1 : 0;
    })
    .slice(0, count);
}
