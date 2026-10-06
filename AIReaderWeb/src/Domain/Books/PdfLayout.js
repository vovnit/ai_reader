// Makes a PDF's text into a book's chapters, `{ title, paragraphs }`, each
// paragraph `{ text, heading }`: one chapter per bookmark, the pages before
// the first making one of their own. Without bookmarks, each heading opens a
// chapter, and without headings either, every `pagesPerChapter` pages do.
// `title` names a chapter nothing else names; chapters with no text are left
// out. The same as the Kindle app's `PdfLayout`.
import { pdfParagraphs } from "./PdfParagraphs.js";

/** How many pages make a chapter when the PDF has no bookmarks to say. */
export const pagesPerChapter = 10;

export function pdfChapters(pages, bookmarks, title) {
  const paragraphs = pdfParagraphs(pages);
  const chapters = [];
  if (!bookmarks.length && paragraphs.filter((paragraph) => paragraph.heading).length >= 2) {
    // No bookmarks, but headings: each run of them opens a chapter.
    chapters.push({ title, paragraphs: [] });
    paragraphs.forEach((paragraph, i) => {
      if (paragraph.heading && (i === 0 || !paragraphs[i - 1].heading)) {
        if (chapters.at(-1).paragraphs.length) chapters.push({ title: paragraph.text, paragraphs: [] });
        else chapters.at(-1).title = paragraph.text;
      }
      chapters.at(-1).paragraphs.push({ text: paragraph.text, heading: paragraph.heading });
    });
    return chapters;
  }

  // A chapter from each bookmark's page, or from every few pages.
  const marks = [...bookmarks].sort((a, b) => a.page - b.page).filter((mark, i, all) => i === 0 || mark.page !== all[i - 1].page);
  const count = pages.length;
  if (!marks.length) {
    for (let start = 0; start < count; start += pagesPerChapter) {
      const end = Math.min(count, start + pagesPerChapter);
      marks.push({ title: count <= pagesPerChapter ? title : `Pages ${start + 1}–${end}`, page: start });
    }
  } else if (marks[0].page > 0) {
    marks.unshift({ title, page: 0 });
  }
  for (const mark of marks) chapters.push({ title: mark.title || title, paragraphs: [] });
  let chapter = 0;
  for (const paragraph of paragraphs) {
    while (chapter + 1 < marks.length && marks[chapter + 1].page <= paragraph.page) chapter++;
    chapters[chapter].paragraphs.push({ text: paragraph.text, heading: paragraph.heading });
  }
  return chapters.filter((draft) => draft.paragraphs.length);
}
