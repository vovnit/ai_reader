// The HTML reader, on books put together here: the same cases as the Kindle
// app's check, since the apps must read an HTML file alike.
import { htmlBook } from "../../src/Domain/Books/HtmlBook.js";
import { check } from "./Check.mjs";
import { outline } from "./TextBook.mjs";

const nested = `<!DOCTYPE html>
<html lang=" FR-fr "><head><meta charset="utf-8">
<meta name="Author" content="  Antoine   de Saint-Exupéry "><meta name="author" content="Second">
<title>  Le   Petit
  Prince </title></head>
<body><div>Loose text
  <div><h1>Le Petit <em>Prince</em></h1><h3>Une histoire</h3>
    <div>Lorsque j’avais <i>six</i> ans
      <div>Nested <span>inner</span> div.</div>
      After it.</div>
    <p>Il dit&nbsp;: «&nbsp;Dessine-moi un mouton&nbsp;»</p></div>
  <section><h2>Chapitre II</h2><article><p>J’ai vécu seul.</p></article></section>
</div></body></html>`;

const scripts = `<html><head><title>Scripts</title>
<style>a < b { }</style><script>if (a < b) { x = "</div>"; }</script></head><body>
<p>Before.</p><script src="x.js"/><p>After a closed script.</p>
<SCRIPT>var s = "<h1>Not a heading</h1>";</SCRIPT><p>After an upper-case one.</p>
<!-- <script> in a comment --><p>After the comment.</p>
<noscript><p>No.</p></noscript><template><p>No.</p></template>
<svg><title>No</title><svg><text>No</text></svg><text>No</text></svg><math><mi>x</mi></math>
<nav><ul><li>No</li><li><nav>No</nav></li><li>No</li></ul></nav><p>Last.</p></body></html>`;

const blocks = `<body><h2>Contents</h2><table><caption>Chapters</caption>
<tr><th>Number</th><th>Title</th></tr><tr><td>I.</td><td>Down the <a href="#1">Hole</a></td></tr></table>
<ul><li>One</li><li>Two<ul><li>Inner</li></ul></li></ul><dl><dt>Term</dt><dd>Meaning</dd></dl>
<pre>
  Some   preformatted
     text
</pre>text<hr>after a rule</body>`;

const breaks = `<html><head><title>Breaks</title></head><body>
<h2><a id="c1"></a>CHAPTER I.<br>
Down the Rabbit-Hole</h2><p>Line one<br/>Line two<BR>&nbsp;<br />Line three</p>
<h3>A minor<br>heading</h3><p>Text<br></br>after an end br.</p></body></html>`;

const broken = `<?xml version="1.0"?><!DOCTYPE html>
<P>An unclosed paragraph
<P>Entities: &amp; &eacute;t&eacute; &#233;&#x2014;&unknown; &lt;tag&gt;
</div></span></p><H2>Upper Case</H2>
<p>Stray <b>bold</i> end</em> tags.
<p>A <![CDATA[<b>cdata</b>]]> and a <!-- comment --> inside.
<div>Unclosed div
<p>No end tags at all`;

const gutenberg = `<html lang="en"><head><title>A Small Book | Project Gutenberg</title></head><body>
<header><h2>The Project Gutenberg eBook of A Small Book</h2>
<p><strong>Title</strong>: A Small Book</p><p><strong>Author</strong>: Jane Doe</p>
<div>*** START OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***</div></header>
<h1>A Small Book</h1><h2>by Jane Doe</h2>
<div class="chapter"><h2>CHAPTER I.<br>Down the Hole</h2><p>Alice was tired.</p></div>
<div class="chapter"><h2>CHAPTER II.</h2><p>Up the hill.</p></div>
<footer><div>*** END OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***</div>
<h2>THE FULL PROJECT GUTENBERG LICENSE</h2><p>The license.</p></footer></body></html>`;

const untitled = `<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="RU"><head><title> </title><meta name="author" content=""/></head>
<body><h1>Мастер<br/>и Маргарита</h1><p>&nbsp;</p><html:p>Однажды весною.</html:p><h2>Глава 1</h2><p>Никогда.</p></body></html>`;

export function checkHtmlBook() {
  const book = htmlBook(nested, "nested");
  check("html: the title, the first author, the language lowercased; nested blocks each end a paragraph",
    book.title === "Le Petit Prince" && book.author === "Antoine de Saint-Exupéry" && book.language === "fr-fr"
      && outline(book) === "Le Petit Prince = Loose text || Le Petit Prince = #Le Petit Prince / #Une histoire / Lorsque j’avais six ans "
        + "/ Nested inner div. / After it. / Il dit\u00a0: «\u00a0Dessine-moi un mouton\u00a0» || Chapitre II = #Chapitre II / J’ai vécu seul.",
    `${book.title} / ${book.author} / ${book.language} / ${outline(book)}`);

  check("html: scripts, styles and other hidden elements say nothing, whatever they hold",
    outline(htmlBook(scripts, "scripts")) === "Scripts = Before. / After a closed script. / After an upper-case one. / After the comment. / Last.",
    outline(htmlBook(scripts, "scripts")));

  check("html: table cells, list items, definitions, preformatted text and a rule each end a paragraph",
    outline(htmlBook(blocks, "blocks")) === "Contents = #Contents / Chapters / Number / Title / I. / Down the Hole / One / Two / Inner "
      + "/ Term / Meaning / Some preformatted text / text / after a rule",
    outline(htmlBook(blocks, "blocks")));

  check("html: a break ends a paragraph, but in a heading it is a space",
    outline(htmlBook(breaks, "breaks")) === "CHAPTER I. Down the Rabbit-Hole = #CHAPTER I. Down the Rabbit-Hole / Line one / Line two "
      + "/ Line three / #A minor heading / Text / after an end br.",
    outline(htmlBook(breaks, "breaks")));

  check("html: unclosed paragraphs, stray end tags, entities, CDATA, comments, upper case, no html or body",
    outline(htmlBook(broken, "broken")) === "broken = An unclosed paragraph / Entities: & été é—&unknown; <tag> "
      + "|| Upper Case = #Upper Case / Stray bold end tags. / A <b>cdata</b> and a inside. / Unclosed div / No end tags at all",
    outline(htmlBook(broken, "broken")));

  const pg = htmlBook(gutenberg, "pg");
  check("html: a Gutenberg book is named by its header and keeps only what lies between its markers; a run of h1 and h2 opens one chapter",
    pg.title === "A Small Book" && pg.author === "Jane Doe" && pg.language === "en"
      && outline(pg) === "A Small Book = #A Small Book / #by Jane Doe / #CHAPTER I. Down the Hole / Alice was tired. "
        + "|| CHAPTER II. = #CHAPTER II. / Up the hill.",
    `${pg.title} / ${pg.author} / ${outline(pg)}`);

  const first = htmlBook(untitled, "untitled");
  check("html: without a title the first h1 names the book; xml:lang; a no-break space alone is no paragraph",
    first.title === "Мастер и Маргарита" && first.author === "" && first.language === "ru"
      && outline(first) === "Мастер и Маргарита = #Мастер и Маргарита / Однажды весною. || Глава 1 = #Глава 1 / Никогда.",
    `${first.title} / ${first.language} / ${outline(first)}`);

  const many = htmlBook(`<p>${Array.from({ length: 130 }, (_, i) => `Paragraph ${i + 1}.`).join("<p>")}<h3>Minor</h3>`, "many");
  const empty = htmlBook("", "empty");
  check("html: without h1 or h2, every 100 paragraphs make a part; an empty file has no chapters",
    many.title === "many" && many.chapters.map((c) => `${c.title}:${c.paragraphs.length}`).join(" ") === "Part 1:100 Part 2:31"
      && empty.title === "empty" && empty.author === "" && empty.language === "" && !empty.chapters.length,
    many.chapters.map((c) => `${c.title}:${c.paragraphs.length}`).join(" "));
}
