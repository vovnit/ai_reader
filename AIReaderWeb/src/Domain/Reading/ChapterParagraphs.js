// A chapter cut into what a page renders: one entry per line of its text,
// either a picture or runs of text with the styles that apply to each.
// Offsets stay those of the chapter's text, so a tap or a page start is
// read straight back into it.
import { imagePlaceholder } from "../Books/HtmlText.js";

/**
 * `[{ start, end, image }]` for a picture's line (the index into
 * `chapter.images`), and `[{ start, end, heading, runs: [{ start, end, kinds }] }]`
 * for text, where `kinds` are the span kinds covering the run.
 */
export function chapterParagraphs(chapter) {
  const text = chapter.text;
  const images = new Map(chapter.images.map((image, index) => [image.offset, index]));
  const result = [];
  for (let start = 0; start <= text.length; ) {
    let end = text.indexOf("\n", start);
    if (end < 0) end = text.length;
    if (end === start + 1 && text[start] === imagePlaceholder && images.has(start)) {
      result.push({ start, end, image: images.get(start) });
    } else {
      result.push({ start, end, ...runs(chapter.spans, start, end) });
    }
    start = end + 1;
  }
  return result;
}

function runs(spans, start, end) {
  const covering = spans.filter((span) => span.start < end && span.end > start);
  const cuts = new Set([start, end]);
  for (const span of covering) {
    if (span.start > start) cuts.add(span.start);
    if (span.end < end) cuts.add(span.end);
  }
  const points = [...cuts].sort((a, b) => a - b);
  const result = [];
  for (let i = 0; i + 1 < points.length; i++) {
    const [from, to] = [points[i], points[i + 1]];
    const kinds = covering.filter((span) => span.start <= from && span.end >= to).map((span) => span.kind);
    result.push({ start: from, end: to, kinds: [...new Set(kinds)] });
  }
  const heading = end > start && covering.some((span) => span.kind === "heading" && span.start <= start && span.end >= end);
  return { heading, runs: result };
}
