// What the model produces for a looked-up word:
// `{ lemma, formNote, meaning, guessed, confidence }`.

export function explanationJson(explanation) {
  return JSON.stringify({
    lemma: explanation.lemma,
    form_note: explanation.formNote,
    meaning: explanation.meaning,
    guessed: explanation.guessed,
    confidence: explanation.confidence,
  });
}

/** Reads the JSON object out of a reply, tolerating text around it; null when there is none. */
export function decodeExplanation(content) {
  const start = content.indexOf("{");
  const end = content.lastIndexOf("}");
  if (start < 0 || end < start) return null;
  let json;
  try {
    json = JSON.parse(content.slice(start, end + 1));
  } catch {
    return null;
  }
  if (!json || typeof json !== "object" || !("meaning" in json)) return null;
  const text = (value) => (typeof value === "string" ? value : "");
  return {
    lemma: text(json.lemma),
    formNote: text(json.form_note),
    meaning: text(json.meaning),
    guessed: json.guessed === true,
    confidence: typeof json.confidence === "number" ? json.confidence : 0,
  };
}
