// The FB2 reader, on books put together here: the same cases as the Kindle
// app's check, since the apps must read a FictionBook alike.
import { fb2Book } from "../../src/Domain/Books/Fb2Book.js";
import { check } from "./Check.mjs";

/** A book's chapters on one line: `title = #heading / paragraph || …`. */
function outline(book) {
  return book.chapters
    .map((chapter) => `${chapter.title} = ${chapter.paragraphs.map((p) => (p.heading ? "#" : "") + p.text).join(" / ")}`)
    .join(" || ");
}

const novel = `<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
<description>
<title-info><author><first-name>Lev</first-name><middle-name></middle-name><last-name>Tolstoy</last-name><nickname>LNT</nickname></author>
<author><first-name>Second</first-name></author><book-title>War and
  Peace</book-title><annotation><p>Not read.</p></annotation><lang>RU</lang></title-info>
<src-title-info><book-title>Война и мир</book-title><lang>ru</lang></src-title-info>
<document-info><author><nickname>scanner</nickname></author></document-info>
</description>
<body>
<title><p>Leo Tolstoy</p></title>
<epigraph><p>An epigraph.</p></epigraph>
<section><title><p>Part One</p></title>
<section><title><p>Chapter 1</p></title><p>Well, <emphasis>Prince</emphasis>.<a l:href="#n1" type="note">[1]</a></p><empty-line/><image l:href="#pic"/></section>
<section><p>An untitled section.</p></section>
</section>
</body>
<body name="notes"><title><p>Notes</p></title><section id="n1"><title><p>1</p></title><p>A note.</p></section></body>
<binary id="pic" content-type="image/png">iVBORw0KGgo=</binary>
</FictionBook>`;

const verses = `<FictionBook><description><title-info><author><nickname>Anon</nickname></author><book-title>Verses</book-title></title-info></description>
<body><section><title><p>Poems</p></title>
<poem><title><p>A Song</p></title><stanza><v>First line,</v><v>second line.</v></stanza><text-author>Anon</text-author><date>1805</date></poem>
<subtitle>* * *</subtitle>
<table><tr><th>Name</th><th>Age</th></tr><tr><td>Pierre</td><td></td><td>20</td></tr></table>
<p>Il dit : « Bonjour »\t et\r\n  part.</p>
</section></body>
<body><section><p>A body with no title.</p></section></body></FictionBook>`;

const broken = `<FictionBook><description><title-info><book-title>Broken</book-title></title-info></description>
<body><section><title>Loose title</title><p>Unclosed<p>Stray</emphasis> end.</p><empty-line><p>After &amp; <![CDATA[<raw>]]></p>
<section><title><p>Cut</p></title><p>Short`;

export function checkFb2() {
  const book = fb2Book(novel, "novel");
  check("fb2 reads title-info: the first author, the title, the language lowercased",
    book.title === "War and Peace" && book.author === "Lev Tolstoy" && book.language === "ru",
    `${book.title} / ${book.author} / ${book.language}`);
  check("fb2 chapters: the opening before the sections, a part with its first chapter, an untitled section, the notes",
    outline(book) === "War and Peace = #Leo Tolstoy / An epigraph. || Chapter 1 = #Part One / #Chapter 1 / Well, Prince.[1] "
      + "|| War and Peace = An untitled section. || Notes = #Notes / #1 / A note.",
    outline(book));

  const poems = fb2Book(verses, "verses");
  check("fb2 verse lines, subtitles and table rows, a no-break space kept, an untitled later body as Notes",
    poems.author === "Anon" && poems.language === ""
      && outline(poems) === "Poems = #Poems / #A Song / First line, / second line. / Anon / 1805 / #* * * / Name | Age / Pierre | 20 "
        + "/ Il dit : « Bonjour » et part. || Notes = A body with no title.",
    outline(poems));

  check("fb2 tolerates unclosed and stray tags and a file cut short",
    outline(fb2Book(broken, "broken")) === "Loose title = #Loose title / Unclosed / Stray end. / After & <raw> || Cut = #Cut / Short",
    outline(fb2Book(broken, "broken")));

  const bodiless = fb2Book(`<FictionBook><description><title-info><author><last-name>Nobody</last-name></author>
<book-title>No Body</book-title></title-info></description></FictionBook>`, "bodiless");
  const empty = fb2Book("", "fallback");
  check("fb2 without a body has no chapters; an empty file takes the fallback title",
    bodiless.title === "No Body" && bodiless.author === "Nobody" && !bodiless.chapters.length
      && empty.title === "fallback" && empty.author === "" && empty.language === "" && !empty.chapters.length);

  const plain = fb2Book("<FictionBook><body><p>One.</p><p>Two.</p></body></FictionBook>", "plain");
  check("fb2 without sections is one chapter titled by the book", outline(plain) === "plain = One. / Two.", outline(plain));
}
