// Everything the offline dictionary knows about one written form:
// `{ query, forms: [{ lemma, partOfSpeech, gender, number, features }],
//    articles: [{ lemma, partOfSpeech, senses, source }] }`.

export function emptyLookup(query) {
  return { query, forms: [], articles: [] };
}

/** A compact plain-text rendering, small enough to put in a prompt. */
export function lookupSummary(lookup) {
  if (!lookup.articles.length && !lookup.forms.length) return `No dictionary entry for “${lookup.query}”.`;
  const lines = [`Dictionary results for “${lookup.query}”:`];
  for (const form of lookup.forms) {
    const grammar = [form.partOfSpeech, form.gender, form.number].filter(Boolean).concat(form.features);
    lines.push(`- form of “${form.lemma}” (${grammar.join(" ")})`);
  }
  const sources = new Set(lookup.articles.map((article) => article.source).filter(Boolean));
  for (const article of lookup.articles) {
    const partOfSpeech = article.partOfSpeech ? ` [${article.partOfSpeech}]` : "";
    const source = sources.size > 1 && article.source ? ` (${article.source})` : "";
    lines.push(`- ${article.lemma}${partOfSpeech}${source}: ${article.senses.join("; ")}`);
  }
  return lines.join("\n");
}
