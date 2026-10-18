// The plain-text reader, on books put together here: the same cases as the
// Kindle app's check, since the apps must read a text file alike.
import { opensChapter, textBook } from "../../src/Domain/Books/TextBook.js";
import { check } from "./Check.mjs";

/** A book's chapters on one line: `title = #heading / paragraph || …`. */
export function outline(book) {
  return book.chapters
    .map((chapter) => `${chapter.title} = ${chapter.paragraphs.map((p) => (p.heading ? "#" : "") + p.text).join(" / ")}`)
    .join(" || ");
}

const gutenberg = [
  "The Project Gutenberg eBook of A Small Book", "", "Title: A   Small\tBook", "", "Author: Jane  Doe", "",
  "*** START OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***", "", "[Illustration]", "",
  "CHAPTER I.", "Down the Hole", "", "Alice was beginning to get very", "tired of sitting.", "",
  "CHAPTER II.", "Up the Hill", "", "So she was considering.", "",
  "End of the Project Gutenberg EBook of A Small Book, by Jane Doe", "",
  "*** END OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***", "", "The license.", "",
].join("\r\n");

const numbered = [
  "", "  A preface that is wrapped", "\tacross two lines.", "", "I", "", "The first.", "", "II.", "",
  "XIV", "", "12", "", "Twelve.", "", "iv", "", "1.5", "", "DID", "", "1234", "", "MMMCMXCIX.", "", "",
].join("\n");

const lines = [
  "A first line", "Chapter 1", "It was a dark night.", "CHAPTER TWO: THE STORM", "Chapter", "The wind howled.",
  "Chapters are not headings.", "Chapter: one", "",
].join("\n");

const russian = [
  "Предисловие.", "ЧАСТЬ ПЕРВАЯ", "I", "Мой дядя.", "II", "Книга лежала на столе.", "Глава первая", "ЧАСТЬ ВТОРАЯ", "I", "Он уехал.",
  "Пролог", "Пришла весна.", "Розділ 3", "Кінець.",
].join("\r");

// A list of contents running into the book: the last two headings name the chapter.
const parts = [
  "Contents", "BOOK ONE: 1805", "CHAPTER I", "CHAPTER II", "BOOK TWO: 1805", "CHAPTER I", "BOOK ONE: 1805", "CHAPTER I", "Well, Prince.",
  "CHAPTER II", "Anna Pavlovna.", "BOOK TWO: 1805", "CHAPTER I", "In October.",
].join("\n\n");

const opening = [
  "XIV", "12.", "MMMCMXCIX", "CHAPTER I.", "Chapter 12: The Return", "Book IV—The Fall", "Part 3-a", "Глава 1", "ГЛАВА ПЕРВАЯ",
  "BOOK ONE: 1805", "Prologue", "Пролог", "Epilogue.", "Chapter:", "Chapter\u00a09", "\u00a0Chapter 9",
];
const notOpening = [
  "1234", "DID", "CIVIL", "IIII", "iv", "1.5", "Part of me wanted to stay.", "Книга лежала на столе.", "Chapter One", "Глава первая",
  "Chapter: one", "Chapter;", "Chapter 1805", "Chapter iv", "Chapters are not headings.", "Book the First",
];

const french = ["CHAPITRE PREMIER", "", "Il dit\u00a0: «\u00a0Bonjour\u00a0»", "", "Première partie", "", "\u00a0\u00a0", "", "ÉPILOGUE", "", "\u00a0Fin\u00a0"].join("\n");

const lengths = [
  "Opening.", `ГЛАВА ${"Ж".repeat(74)}`, "Text.", `ГЛАВА ${"Ж".repeat(75)}`, "e\u0301pilogue", "\u212Aapitel 5",
].join("\n");

export function checkTextBook() {
  const book = textBook(gutenberg, "fallback");
  check("text: a Gutenberg file names the book and keeps only what lies between its markers",
    book.title === "A Small Book" && book.author === "Jane Doe" && book.language === ""
      && outline(book) === "A Small Book = [Illustration] || CHAPTER I. Down the Hole = #CHAPTER I. Down the Hole / "
        + "Alice was beginning to get very tired of sitting. || CHAPTER II. Up the Hill = #CHAPTER II. Up the Hill / So she was considering.",
    `${book.title} / ${book.author} / ${outline(book)}`);

  check("text: wrapped lines joined; numbers and standard upper-case roman numerals open chapters; a run of headings is titled by its last two",
    outline(textBook(numbered, "Numbers")) === "Numbers = A preface that is wrapped across two lines. || I = #I / The first. "
      + "|| XIV — 12 = #II. / #XIV / #12 / Twelve. / iv / 1.5 / DID / 1234 || MMMCMXCIX. = #MMMCMXCIX.",
    outline(textBook(numbered, "Numbers")));

  check("text: one paragraph per line without blank lines; a chapter word alone, before a number or in capitals",
    outline(textBook(lines, "Lines")) === "Lines = A first line || Chapter 1 = #Chapter 1 / It was a dark night. "
      + "|| CHAPTER TWO: THE STORM — Chapter = #CHAPTER TWO: THE STORM / #Chapter / The wind howled. / Chapters are not headings. / Chapter: one",
    outline(textBook(lines, "Lines")));

  check("text: Russian and Ukrainian headings, a part's joined to its first chapter's, CR line ends",
    outline(textBook(russian, "Ru")) === "Ru = Предисловие. || ЧАСТЬ ПЕРВАЯ — I = #ЧАСТЬ ПЕРВАЯ / #I / Мой дядя. "
      + "|| II = #II / Книга лежала на столе. / Глава первая || ЧАСТЬ ВТОРАЯ — I = #ЧАСТЬ ВТОРАЯ / #I / Он уехал. "
      + "|| Пролог = #Пролог / Пришла весна. || Розділ 3 = #Розділ 3 / Кінець.",
    outline(textBook(russian, "Ru")));

  check("text: French headings; no-break spaces kept, a line of them blank",
    outline(textBook(french, "Fr")) === "CHAPITRE PREMIER = #CHAPITRE PREMIER / Il dit\u00a0: «\u00a0Bonjour\u00a0» / Première partie "
      + "|| ÉPILOGUE = #ÉPILOGUE / \u00a0Fin\u00a0",
    outline(textBook(french, "Fr")));

  check("text: a heading is at most 80 code points, its word spelled alike",
    outline(textBook(lengths, "Len")) === `Len = Opening. || ГЛАВА ${"Ж".repeat(74)} = #ГЛАВА ${"Ж".repeat(74)} / Text. / ГЛАВА ${"Ж".repeat(75)} `
      + "/ e\u0301pilogue || \u212Aapitel 5 = #\u212Aapitel 5",
    outline(textBook(lengths, "Len")));

  check("text: what opens a chapter and what does not; a no-break space parts words",
    opening.every(opensChapter) && !notOpening.some(opensChapter),
    [...opening.filter((text) => !opensChapter(text)), ...notOpening.filter(opensChapter)].join(" | "));

  check("text: a list of contents running into the book is titled by its last two headings",
    outline(textBook(parts, "Parts")) === "Parts = Contents || BOOK ONE: 1805 — CHAPTER I = #BOOK ONE: 1805 / #CHAPTER I / #CHAPTER II "
      + "/ #BOOK TWO: 1805 / #CHAPTER I / #BOOK ONE: 1805 / #CHAPTER I / Well, Prince. || CHAPTER II = #CHAPTER II / Anna Pavlovna. "
      + "|| BOOK TWO: 1805 — CHAPTER I = #BOOK TWO: 1805 / #CHAPTER I / In October.",
    outline(textBook(parts, "Parts")));

  const plain = (count) => Array.from({ length: count }, (_, i) => `Paragraph ${i + 1}.`).join("\n\n");
  const cut = textBook(plain(250), "Parts").chapters;
  const one = textBook(plain(100), "One").chapters;
  check("text: without headings, every 100 paragraphs make a part; 100 or fewer are one chapter named by the book",
    cut.map((c) => `${c.title}:${c.paragraphs.length}`).join(" ") === "Part 1:100 Part 2:100 Part 3:50"
      && cut[1].paragraphs[0].text === "Paragraph 101." && one.length === 1 && one[0].title === "One",
    cut.map((c) => `${c.title}:${c.paragraphs.length}`).join(" "));

  const empty = textBook("", "Empty");
  const startOnly = textBook("Title: Not Taken\n*** START OF SOMETHING ***\nBody.", "Start");
  check("text: an empty file has no chapters; a start marker without an end is not Gutenberg's",
    empty.title === "Empty" && empty.author === "" && !empty.chapters.length
      && startOnly.title === "Start" && outline(startOnly) === "Start = Title: Not Taken / *** START OF SOMETHING *** / Body.",
    outline(startOnly));
}
