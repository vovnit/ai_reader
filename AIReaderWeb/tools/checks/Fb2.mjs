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

// A notes body before the text, which is the first body without a name;
// parts whose chapters restart at I; spacer paragraphs of no-break spaces.
const volumes = `<FictionBook><description><title-info><book-title>Война и мир</book-title></title-info></description>
<body name="notes"><title><p>Примечания</p></title><section><p>Сноска.</p></section></body>
<body><title><p>Лев Толстой</p></title>
<section><title><p>ТОМ ПЕРВЫЙ</p></title>
<section><title><p>ЧАСТЬ ПЕРВАЯ</p></title>
<section><title><p>I</p></title><p>&#160;</p><p>— Eh bien, mon prince.</p><p> &#160; </p></section>
<section><title><p>II</p></title><table><tr><td>&#160;</td><td>Да</td></tr><tr><td>&#160;</td></tr></table></section>
</section>
<section><title><p>ЧАСТЬ ВТОРАЯ</p></title>
<section><title><p>I</p></title><p>В октябре.</p></section>
</section></section>
<section><section><title><p>&#160;</p><p>ЭПИЛОГ</p></title><p>Прошло семь лет.</p></section></section>
</body>
<body name="comments"><section><p>Комментарий.</p></section></body>
<body><p>A second body without a name.</p></body></FictionBook>`;

const broken = `<FictionBook><description><title-info><book-title>Broken</book-title></title-info></description>
<body><section><title>Loose title</title><p>Unclosed<p>Stray</emphasis> end.</p><empty-line><p>After &amp; <![CDATA[<raw>]]></p>
<section><title><p>Cut</p></title><p>Short`;

export function checkFb2() {
  const book = fb2Book(novel, "novel");
  check("fb2 reads title-info: the first author, the title, the language lowercased",
    book.title === "War and Peace" && book.author === "Lev Tolstoy" && book.language === "ru",
    `${book.title} / ${book.author} / ${book.language}`);
  check("fb2 chapters: the opening before the sections, a part with its first chapter titled by both, an untitled section, the notes",
    outline(book) === "War and Peace = #Leo Tolstoy / An epigraph. || Part One — Chapter 1 = #Part One / #Chapter 1 / Well, Prince.[1] "
      + "|| War and Peace = An untitled section. || Notes = #Notes / #1 / A note.",
    outline(book));

  const poems = fb2Book(verses, "verses");
  check("fb2 verse lines, subtitles and table rows, a no-break space kept, an untitled later body as Notes",
    poems.author === "Anon" && poems.language === ""
      && outline(poems) === "Poems = #Poems / #A Song / First line, / second line. / Anon / 1805 / #* * * / Name | Age / Pierre | 20 "
        + "/ Il dit : « Bonjour » et part. || Notes = A body with no title.",
    outline(poems));

  check("fb2: the text is the first body without a name, the others follow in file order; a part's title joins its first chapter's, the volume's above it left out; paragraphs of no-break spaces dropped",
    outline(fb2Book(volumes, "volumes")) === "ЧАСТЬ ПЕРВАЯ — I = #Лев Толстой / #ТОМ ПЕРВЫЙ / #ЧАСТЬ ПЕРВАЯ / #I / — Eh bien, mon prince. "
      + "|| II = #II / Да || ЧАСТЬ ВТОРАЯ — I = #ЧАСТЬ ВТОРАЯ / #I / В октябре. || ЭПИЛОГ = #ЭПИЛОГ / Прошло семь лет. "
      + "|| Примечания = #Примечания / Сноска. || Notes = Комментарий. || Notes = A second body without a name.",
    outline(fb2Book(volumes, "volumes")));

  const named = fb2Book(`<FictionBook><body name="main"><section><title><p>Part One</p></title><section><title><p>Chapter 1</p></title>
<p>Text.</p></section></section></body><body name="notes"><section><p>A note.</p></section></body></FictionBook>`, "named");
  check("fb2: when every body has a name, the first is the text",
    outline(named) === "Part One — Chapter 1 = #Part One / #Chapter 1 / Text. || Notes = A note.", outline(named));

  check("fb2 tolerates unclosed and stray tags and a file cut short",
    outline(fb2Book(broken, "broken")) === "Loose title = #Loose title / Unclosed / Stray end. / After & <raw> || Cut = #Cut / Short",
    outline(fb2Book(broken, "broken")));

  const bodiless = fb2Book(`<FictionBook><description><title-info><author><last-name>Nobody</last-name></author>
<book-title>No Body</book-title></title-info></description></FictionBook>`, "bodiless");
  const empty = fb2Book("", "fallback");
  check("fb2 without a body has no chapters; an empty file takes the fallback title",
    bodiless.title === "No Body" && bodiless.author === "Nobody" && !bodiless.chapters.length
      && empty.title === "fallback" && empty.author === "" && empty.language === "" && !empty.chapters.length);

  const album = fb2Book(`<FictionBook><body><section><title><p>Page 1</p></title><image l:href="#p1"/></section>
<section><title><p>Page 2</p></title><image l:href="#p2"/></section><section><title><p>Page 3</p></title><image l:href="#p3"/></section>
<section><p>Text.</p></section></body></FictionBook>`, "album");
  check("fb2: a run of sections with nothing but titles is titled by its last two",
    outline(album) === "Page 2 — Page 3 = #Page 1 / #Page 2 / #Page 3 / Text.", outline(album));

  const plain = fb2Book("<FictionBook><body><p>One.</p><p>Two.</p></body></FictionBook>", "plain");
  check("fb2 without sections is one chapter titled by the book", outline(plain) === "plain = One. / Two.", outline(plain));
}
