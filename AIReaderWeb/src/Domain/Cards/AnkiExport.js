// Cards as the text file Anki imports: one note per line, fields a tab
// apart, under the header lines newer Anki reads and older Anki skips. The
// front is the word with its lemma and the sentence it was met in; the back
// is the meaning it had there.

/** Fields are read as HTML, so what would pass for markup is escaped, and a field stays on its line. */
function field(text) {
  return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/[\t\n\r]/g, " ");
}

export function ankiText(cards, deck) {
  let out = `#separator:tab\n#html:true\n#deck:${deck.replace(/[\t\n\r]/g, " ")}\n`;
  for (const card of cards) {
    let front = `<b>${field(card.front)}</b>`;
    if (card.lemma) front += ` (${field(card.lemma)})`;
    if (card.sentence) front += `<br><i>${field(card.sentence)}</i>`;
    out += `${front}\t${field(card.back)}\n`;
  }
  return out;
}
