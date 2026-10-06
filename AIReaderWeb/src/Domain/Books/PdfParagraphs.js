// Turns a PDF's lines back into paragraphs, undoing what the page did:
// running heads and page numbers are dropped, lines are joined — a word
// hyphenated at a line's end mended — and a paragraph a page break cut is
// joined again. A line set larger than the text is a heading. The same
// rules as the Kindle app's `PdfParagraphs`. A paragraph is
// `{ text, heading, page }`.

/** The size most of the book's letters are set in, and how far apart its lines usually are. */
function measureOf(pages) {
  const letters = new Map();
  let total = 0;
  for (const page of pages) {
    for (const line of page) {
      const size = Math.round(line.size * 10) / 10;
      letters.set(size, (letters.get(size) ?? 0) + line.text.length);
      total += line.text.length;
    }
  }
  const measure = { size: 10, gap: 12 };
  let counted = 0;
  for (const [size, count] of [...letters].sort((a, b) => a[0] - b[0])) {
    counted += count;
    if (counted * 2 >= total) {
      measure.size = size;
      break;
    }
  }
  const gaps = [];
  for (const page of pages) {
    for (let i = 1; i < page.length; i++) {
      const gap = page[i].y - page[i - 1].y;
      if (isBody(page[i], measure) && isBody(page[i - 1], measure) && gap > 0 && gap < measure.size * 3) gaps.push(gap);
    }
  }
  gaps.sort((a, b) => a - b);
  measure.gap = gaps.length ? gaps[Math.floor(gaps.length / 2)] : measure.size * 1.2;
  return measure;
}

const isBody = (line, measure) => Math.abs(line.size - measure.size) < measure.size * 0.15;
const isHeading = (line, measure) => line.size > measure.size * 1.2;

/** A line as it is compared with other pages' lines: in lower case, its numbers and extra spaces gone. */
const normalized = (text) => text.replace(/[0-9]/g, "").replace(/[A-Z]/g, (c) => c.toLowerCase()).replace(/[\t\n\v\f\r ]+/g, " ").replace(/^ | $/g, "");
const words = (text) => text.split(" ").filter(Boolean).length;

/** A page number: digits or a roman numeral, perhaps between dashes. */
function isPageNumber(text) {
  const bare = text.replace(/[ \-–—|.·]/g, "");
  return bare.length > 0 && bare.length <= 7 && (/^[0-9]+$/.test(bare) || /^[ivxlcdm]+$/i.test(bare));
}

/** Whether a paragraph ends as a sentence does, rather than being cut. */
const finished = (text) => /[.!?:;")\]…»”]$/.test(text);
const startsLowercase = (text) => /^\p{Ll}/u.test(text);
const startsLetter = (text) => /^\p{L}/u.test(text);

/** A paragraph with a line added; a word broken with a hyphen at the line's end is mended. */
function joined(paragraph, line) {
  for (const hyphen of ["-", "‐", "­"]) {
    if (paragraph.endsWith(hyphen) && (hyphen === "­" || startsLowercase(line))) return paragraph.slice(0, -1) + line;
  }
  return `${paragraph} ${line}`;
}

/**
 * The text with single spaces, and without the object replacement character
 * a picture leaves in some PDFs' text, which the reader keeps for its own
 * pictures.
 */
const collapsed = (text) => text.replaceAll("￼", "").replace(/ {2,}/g, " ").replace(/^ | $/g, "");

/**
 * Where the text block starts, or ends: the outermost place at least two
 * body lines agree on, so one stray line cannot move it; the outermost line
 * when no two agree.
 */
function edge(lines, measure, left) {
  const counts = new Map();
  let outermost = left ? Infinity : -Infinity;
  for (const line of lines) {
    if (!isBody(line, measure)) continue;
    const x = left ? line.left : line.right;
    // Rounded half away from zero, as C's lround is.
    const key = Math.sign(x) * Math.round(Math.abs(x));
    counts.set(key, (counts.get(key) ?? 0) + 1);
    outermost = left ? Math.min(outermost, x) : Math.max(outermost, x);
  }
  const agreed = [...counts].filter(([, count]) => count >= 2).map(([x]) => x).sort((a, b) => a - b);
  return agreed.length ? (left ? agreed[0] : agreed.at(-1)) : outermost;
}

/**
 * The lines nearest the top and the foot of a page, outermost first, each
 * with how far it stands from the next line in: where running heads and
 * page numbers are. `[index, apart]` pairs; the top two come first.
 */
function edges(lines) {
  const order = lines.map((_, index) => index).sort((a, b) => lines[a].y - lines[b].y || a - b);
  const found = [];
  const count = order.length;
  for (let k = 0; k < 2 && k < count; k++) {
    found.push([order[k], k + 1 < count ? lines[order[k + 1]].y - lines[order[k]].y : Infinity]);
  }
  for (let k = 0; k < 2 && k + 2 < count; k++) {
    const at = count - 1 - k;
    found.push([order[at], lines[order[at]].y - lines[order[at - 1]].y]);
  }
  return found;
}

/** The book's paragraphs, from its pages' lines as `pageLines` reads them. */
export function pdfParagraphs(pages) {
  const measure = measureOf(pages);
  // A line repeated at the top or foot of three pages or more, and set
  // apart from the text, is a running head.
  const repeats = new Map();
  for (const page of pages) {
    const seen = new Set();
    for (const [index] of edges(page)) {
      const key = normalized(page[index].text);
      if (key && !seen.has(key)) {
        seen.add(key);
        repeats.set(key, (repeats.get(key) ?? 0) + 1);
      }
    }
  }
  const isFurniture = (line, apart) => {
    if (isHeading(line, measure)) return false;
    const key = normalized(line.text);
    return isPageNumber(line.text) || ((repeats.get(key) ?? 0) >= 3 && apart > measure.gap * 1.3 && words(key) <= 8);
  };

  const found = [];
  pages.forEach((all, number) => {
    const dropped = new Set();
    const outer = edges(all);
    // The top two, then the foot two: each stops at the first line that
    // is the book's own.
    for (let k = 0; k < outer.length && k < 2 && isFurniture(all[outer[k][0]], outer[k][1]); k++) dropped.add(outer[k][0]);
    for (let k = 2; k < outer.length && isFurniture(all[outer[k][0]], outer[k][1]); k++) dropped.add(outer[k][0]);
    const lines = all.filter((_, index) => !dropped.has(index));
    const left = edge(lines, measure, true);
    const right = edge(lines, measure, false);
    lines.forEach((line, i) => {
      const heading = isHeading(line, measure);
      let starts;
      if (i === 0) {
        // A paragraph the page break cut goes on, unindented, in lower case
        // — or with any word, when it was long and unfinished.
        const last = found.at(-1);
        const open = last && !last.heading && !finished(last.text) ? last : null;
        const indented = line.left - left > measure.size * 0.8;
        starts = !open || heading || indented
          || !(startsLowercase(line.text) || (new TextEncoder().encode(open.text).length >= 100 && startsLetter(line.text)));
      } else {
        const above = lines[i - 1];
        const gap = line.y - above.y;
        if (heading && isHeading(above, measure)) {
          starts = gap > line.size * 2;
        } else {
          starts = heading !== isHeading(above, measure)
            || gap > measure.gap * 1.6 || gap < -measure.gap * 0.5
            || line.left - above.left > measure.size * 0.8
            // Two one-line paragraphs, both indented, the first a whole sentence.
            || (line.left - left > measure.size * 0.8 && Math.abs(line.left - above.left) <= measure.size * 0.8 && finished(above.text))
            || (above.right < right - measure.size * 2.5 && finished(above.text));
        }
      }
      if (starts) found.push({ text: line.text, heading, page: number });
      else found.at(-1).text = joined(found.at(-1).text, line.text);
    });
  });
  // Nothing but whitespace, a no-break space included, is no paragraph.
  return found.map((paragraph) => ({ ...paragraph, text: collapsed(paragraph.text) })).filter((paragraph) => /\S/.test(paragraph.text));
}
