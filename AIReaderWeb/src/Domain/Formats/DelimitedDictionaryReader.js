// Word lists: one headword and its definitions per line, separated by tabs
// or by commas. The plainest thing a reader is likely to have made.

/** Splits a line, honouring the double quotes a spreadsheet export adds. */
function fields(line, separator) {
  const result = [];
  let field = "";
  let quoted = false;
  for (const c of line) {
    if (c === '"') quoted = !quoted;
    else if (c === separator && !quoted) {
      result.push(field.trim());
      field = "";
    } else field += c;
  }
  result.push(field.trim());
  return result.filter(Boolean);
}

/** Hands `entry({ headword, partOfSpeech, senses })` each line that has a definition. */
export function readDelimited(contents, entry) {
  const lines = contents.split("\n");
  // Tabs when the file uses any, commas otherwise.
  const separator = lines.slice(0, 20).some((line) => line.includes("\t")) ? "\t" : ",";
  for (const raw of lines) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const parts = fields(line, separator);
    if (parts.length < 2) continue;
    entry({ headword: parts[0], partOfSpeech: "", senses: parts.slice(1) });
  }
}
