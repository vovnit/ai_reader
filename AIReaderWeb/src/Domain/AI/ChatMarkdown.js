// A model's answer, which is usually Markdown, made readable line by line,
// as the iOS app's `ChatMarkdown` does: headings in bold, bullets as •,
// quotes set off, fences kept as code, and bold, italics, code and links
// within a line. Lines are `{ kind, prefix, runs }`, runs
// `{ text, bold, italic, code, href }`; the view turns them into text, never
// into markup.

export function markdownLines(markdown) {
  const lines = [];
  let inFence = false;
  for (const line of markdown.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (trimmed.startsWith("```")) {
      inFence = !inFence;
      continue;
    }
    if (inFence) {
      lines.push({ kind: "code", prefix: "", runs: [{ text: line, code: true }] });
      continue;
    }
    const heading = trimmed.match(/^#{1,6}\s+(.*)$/);
    if (heading) {
      lines.push({ kind: "heading", prefix: "", runs: inline(heading[1]).map((run) => ({ ...run, bold: true })) });
    } else if (/^([-*_]\s*){3,}$/.test(trimmed)) {
      lines.push({ kind: "rule", prefix: "", runs: [] });
    } else {
      const indent = line.match(/^[ \t]*/)[0];
      const bullet = trimmed.match(/^[-*+]\s+(.*)$/);
      const quote = trimmed.match(/^>\s?(.*)$/);
      if (bullet) lines.push({ kind: "text", prefix: `${indent}• `, runs: inline(bullet[1]) });
      else if (quote) lines.push({ kind: "quote", prefix: "│ ", runs: inline(quote[1]).map((run) => ({ ...run, italic: true })) });
      // Numbered items keep their numbers as written.
      else lines.push({ kind: "text", prefix: indent, runs: inline(trimmed) });
    }
  }
  return lines;
}

const pattern = /(\*\*|__)(.+?)\1|(\*|_)(?!\s)(.+?)(?<!\s)\3|`([^`]+)`|\[([^\]]+)\]\(([^)\s]+)\)/g;

/** Bold, italics, code and links within one line. */
export function inline(text) {
  const runs = [];
  let last = 0;
  for (const match of text.matchAll(pattern)) {
    if (match.index > last) runs.push({ text: text.slice(last, match.index) });
    if (match[2] !== undefined) runs.push(...inline(match[2]).map((run) => ({ ...run, bold: true })));
    else if (match[4] !== undefined) runs.push(...inline(match[4]).map((run) => ({ ...run, italic: true })));
    else if (match[5] !== undefined) runs.push({ text: match[5], code: true });
    else runs.push({ text: match[6], href: match[7] });
    last = match.index + match[0].length;
  }
  if (last < text.length) runs.push({ text: text.slice(last) });
  return runs;
}
