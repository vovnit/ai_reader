// Makes the paragraphs of a plain-text or HTML book, `{ text, heading, opens }`,
// into its chapters, `{ title, paragraphs }`: each run of paragraphs that
// open a chapter starts one, titled by the run's last two joined with " — ",
// and the paragraphs before the first make one titled with the book's title.
// A book where nothing opens a chapter is cut every `paragraphsPerPart`
// paragraphs. Chapters with no paragraphs are left out. The same as
// `TextChapters.cpp` (Kindle) and `TextChapters.swift` (iOS).
//
// Books are matched across devices by title and author, and reading places
// by chapter number and text, so the three apps must read a file to the same
// strings: hence one whitespace rule, here, for both readers.

/** How many paragraphs make a chapter when nothing in the book opens one. */
export const paragraphsPerPart = 100;

const isSpace = (c) => c === " " || c === "\t" || c === "\r" || c === "\n";

/** Runs of space, tab, CR and LF made one space and trimmed; a no-break space, which French sets before ":", stays. */
export function collapsed(text) {
  let out = "";
  let space = false;
  for (const c of text) {
    if (isSpace(c)) {
      space = out !== "";
      continue;
    }
    if (space) out += " ";
    space = false;
    out += c;
  }
  return out;
}

/** True when nothing but those spaces and no-break spaces is there: an `&nbsp;` keeps a paragraph open in HTML but says nothing. */
export function isBlank(text) {
  for (const c of text) if (!isSpace(c) && c !== "\u00a0") return false;
  return true;
}

export function textChapters(paragraphs, title) {
  const chapters = [];
  const bare = ({ text, heading }) => ({ text, heading });
  if (paragraphs.some((paragraph) => paragraph.opens)) {
    chapters.push({ title, paragraphs: [] });
    // The run of headings being read. Its last two name the chapter: a part's
    // heading and its first chapter's ("BOOK ONE: 1805 — CHAPTER I"), so the
    // contents still show where a part starts when its chapters restart at I.
    // Any before them are a list of contents running into the book.
    let run = [];
    paragraphs.forEach((paragraph, i) => {
      if (paragraph.opens) {
        if (i === 0 || !paragraphs[i - 1].opens) {
          if (chapters.at(-1).paragraphs.length) chapters.push({ title: "", paragraphs: [] });
          run = [];
        }
        run.push(paragraph.text);
        chapters.at(-1).title = run.slice(-2).join(" — ");
      }
      chapters.at(-1).paragraphs.push(bare(paragraph));
    });
    return chapters;
  }
  for (let start = 0; start < paragraphs.length; start += paragraphsPerPart) {
    chapters.push({
      title: paragraphs.length <= paragraphsPerPart ? title : `Part ${start / paragraphsPerPart + 1}`,
      paragraphs: paragraphs.slice(start, start + paragraphsPerPart).map(bare),
    });
  }
  return chapters;
}
