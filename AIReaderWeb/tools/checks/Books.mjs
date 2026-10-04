// The book's structure: package, navigation, chapter text, language,
// reading place — the Kindle check's cases, with the same expectations.
import { bookKey } from "../../src/Domain/Books/BookKey.js";
import { contentsEntryAt, parseNavigation } from "../../src/Domain/Books/EpubNavigation.js";
import { coverItem, navigationItem, packagePath, parsePackage, readingOrder, resolve } from "../../src/Domain/Books/EpubPackage.js";
import { plainText } from "../../src/Domain/Books/HtmlText.js";
import { detectLanguage, languageCode } from "../../src/Domain/Books/LanguageDetector.js";
import { placeAt, resolvePlace } from "../../src/Domain/Books/ReadingPlace.js";
import { isRemoteBook, remoteBookName } from "../../src/Domain/Books/RemoteBookName.js";
import { remoteBookName as extensionName } from "../../../BrowserExtension/lib/names.js";
import { check } from "./Check.mjs";

const spanned = (text, kind) => text.spans.filter((span) => span.kind === kind).map((span) => `${text.text.slice(span.start, span.end)}|`).join("");

export function checkBooks() {
  const container = '<?xml version="1.0"?><container><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>';
  check("container.xml names the package", packagePath(container) === "OEBPS/content.opf");
  const pkg = parsePackage(`<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" xmlns:dc="http://purl.org/dc/elements/1.1/">
  <metadata><dc:title>Le Petit Prince</dc:title><dc:creator>Antoine de Saint-Exup&#233;ry</dc:creator><dc:language>fr</dc:language>
    <meta name="cover" content="cover-img"/></metadata>
  <manifest>
    <item id="cover-img" href="images/cover.jpg" media-type="image/jpeg"/>
    <item id="ch1" href="text/ch%201.xhtml" media-type="application/xhtml+xml"/>
    <item id="ch2" href="text/ch2.xhtml" media-type="application/xhtml+xml"/>
    <item id="css" href="style.css" media-type="text/css"/>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx"><itemref idref="ch1"/><itemref idref="css"/><itemref idref="ch2"/></spine>
</package>`);
  check("package title, author, language", pkg.title === "Le Petit Prince" && pkg.author === "Antoine de Saint-Exupéry" && pkg.language === "fr");
  const order = readingOrder(pkg);
  check("reading order keeps only markup", order.length === 2 && order[0].href === "text/ch 1.xhtml" && order[1].id === "ch2");
  check("cover from EPUB 2 meta", coverItem(pkg)?.href === "images/cover.jpg");
  check("hrefs resolve against the package folder", resolve("../images/a.jpg", "OEBPS/text") === "OEBPS/images/a.jpg");
  check("navigation from the EPUB 2 spine", navigationItem(pkg)?.href === "toc.ncx");
  pkg.items.set("nav", { id: "nav", href: "nav.xhtml", mediaType: "application/xhtml+xml", properties: "scripted nav" });
  check("navigation prefers the EPUB 3 document", navigationItem(pkg)?.id === "nav");

  let entries = parseNavigation(`<?xml version="1.0"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/"><navMap>
<navPoint id="n1"><navLabel><text> PREMIER CHAPITRE </text></navLabel><content src="Text/ch1.xhtml"/>
  <navPoint id="n1a"><navLabel><text>Un</text></navLabel><content src="Text/ch1.xhtml#un"/></navPoint>
</navPoint>
<navPoint id="n2"><navLabel><text>CHAPITRE II</text></navLabel><content src="Text/ch2.xhtml"/></navPoint>
<navPoint id="n3"><navLabel><text></text></navLabel><content src="Text/ch3.xhtml"/></navPoint>
</navMap></ncx>`);
  check("ncx entries, nested and trimmed, blank ones dropped", entries.length === 3 && entries[0].title === "PREMIER CHAPITRE"
    && entries[0].depth === 0 && entries[1].title === "Un" && entries[1].href === "Text/ch1.xhtml#un" && entries[1].depth === 1 && entries[2].depth === 0);
  entries = parseNavigation(`<html xmlns:epub="http://www.idpf.org/2007/ops"><body>
<nav epub:type="landmarks"><ol><li><a href="cover.xhtml">Cover</a></li></ol></nav>
<nav epub:type="toc"><h1>Contents</h1><ol>
<li><a href="ch1.xhtml">Chapitre <span>I</span></a><ol><li><a href="ch1.xhtml#a">Partie A</a></li></ol></li>
<li><span>Sans lien</span></li>
<li><a href="ch2.xhtml">Chapitre II</a></li>
</ol></nav></body></html>`);
  check("nav toc entries, the landmarks and a linkless heading left out", entries.length === 3 && entries[0].title === "Chapitre I"
    && entries[1].title === "Partie A" && entries[1].depth === 1 && entries[2].title === "Chapitre II" && entries[2].depth === 0, JSON.stringify(entries));
  const contents = [{ title: "One", chapter: 0, offset: 0 }, { title: "One b", chapter: 0, offset: 40 }, { title: "Two", chapter: 2, offset: 0 }];
  check("the entry a place falls under", contentsEntryAt(contents, 0, 10) === 0 && contentsEntryAt(contents, 0, 40) === 1
    && contentsEntryAt(contents, 1, 0) === 1 && contentsEntryAt(contents, 2, 5) === 2 && contentsEntryAt(contents, 9, 0) === 2);
  check("no entry before the first", contentsEntryAt([], 0, 0) === -1);

  const text = plainText(`<html><head><title>Ignored</title><style>p{}</style></head>
<body><h1>Chapitre  I</h1>
<p>Lorsque j'avais six ans j'ai vu, une fois, une <i>magnifique</i>
image.</p><p>Elle repr&eacute;sentait un serpent boa.<br/>Fin.</p><!-- note --></body></html>`);
  check("html becomes paragraphs", text.text === "Chapitre I\nLorsque j'avais six ans j'ai vu, une fois, une magnifique image.\nElle représentait un serpent boa.\nFin.", text.text);
  check("html keeps heading and italic spans", spanned(text, "heading") === "Chapitre I|" && spanned(text, "italic") === "magnifique|");
  const entitiesText = plainText("<p>Un c&oelig;ur &agrave; l&rsquo;&Eacute;cole&nbsp;! &uuml; &#8212; &euro; &shy;a&#173;b</p>");
  check("html decodes named entities, soft hyphens dropped", entitiesText.text === "Un cœur à l’École ! ü — € ab", entitiesText.text);
  const notes = plainText(`<body>
<p>Une maison<a href="notes.xhtml#n1" epub:type="noteref"><sup>1</sup></a> vieille, un mot<sup><a href="#fn2">[2]</a></sup> et<a class="c" href="p12.html#n3"><sup>3</sup></a> une note<a href="#fn4">*</a> ici.</p>
<p>Voir <a href="#c3">le chapitre III</a> et le XIX<sup>e</sup> si&egrave;cle, H<sub>2</sub>O.</p>
<aside epub:type="footnote" id="fn2"><p><a href="#r2">2</a> La note deux. <a href="#r2" epub:type="backlink">&#8617;</a></p></aside>
</body>`);
  check("html drops footnote references but keeps a note's own number",
    notes.text === "Une maison vieille, un mot et une note ici.\nVoir le chapitre III et le XIXe siècle, H2O.\n2 La note deux.", notes.text);
  check("html keeps superscripts and subscripts, minus the dropped ones", spanned(notes, "superscript") === "e|" && spanned(notes, "subscript") === "2|");
  const shape = plainText(`<body>
<nav epub:type="landmarks" hidden=""><ol><li><a href="cover.xhtml">Cover</a></li></ol></nav>
<nav epub:type="toc"><ol><li><a href="ch1.xhtml">Chapitre I</a></li></ol></nav>
<span epub:type="pagebreak" title="12">12</span><p>Avant.</p><hr/><p>Apr&egrave;s, <cite>Titre</cite> et <q>dit</q>.</p>
<p>Vers un<br/>
  vers deux</p><p>&nbsp;</p><hr/></body>`);
  check("html hides hidden elements and page numbers, blank line at a rule",
    shape.text === "Chapitre I\nAvant.\n\nAprès, Titre et “dit”.\nVers un\nvers deux\n ", JSON.stringify(shape.text));
  check("html sets cite in italics", spanned(shape, "italic") === "Titre|");
  const anchored = plainText('<body><p>Avant.</p><h2 id="ch2">Deux</h2><p><a name="old"/>Texte.</p></body>');
  check("html records where ids and named anchors begin", anchored.text.substr(anchored.anchors.get("ch2"), 4) === "Deux"
    && anchored.text.substr(anchored.anchors.get("old"), 5) === "Texte");

  const french = { text: "Lorsque j'avais six ans j'ai vu, une fois, une magnifique image, dans un livre sur la Forêt Vierge. Ça représentait un serpent boa qui avalait un fauve. On disait dans le livre que les serpents boas avalent leur proie tout entière, sans la mâcher. ".repeat(12) };
  const english = { text: "Once when I was six years old I saw a magnificent picture in a book about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal, and the book said that they swallow their prey whole. ".repeat(12) };
  check("detects French prose", detectLanguage([french]) === "fr", detectLanguage([french]));
  check("detects English prose", detectLanguage([english]) === "en", detectLanguage([english]));
  check("says nothing about a short text", detectLanguage([{ text: "Bonjour." }]) === "");
  check("normalizes declared codes", languageCode("fr-FR") === "fr" && languageCode("EN") === "en" && languageCode("fre") === "fr");

  const chapter = "Première phrase du chapitre. Deuxième phrase, un peu plus longue, qui continue. Troisième.";
  const offset = chapter.indexOf("Deuxième");
  const place = placeAt(3, chapter, offset);
  check("place keeps whole words", place.snippet.startsWith("Deuxième phrase") && !place.snippet.endsWith(" ") && place.chapter === 3, place.snippet);
  check("place resolves by its words", resolvePlace(place, chapter) === offset);
  check("place falls back to the fraction", resolvePlace({ chapter: 0, fraction: 0.5, snippet: "absent" }, "0123456789") === 5);
  check("place survives a re-rendering", resolvePlace(place, `Prologue ajouté. ${chapter}`) === offset + "Prologue ajouté. ".length);
  const ios = "￼\nAlors j’ai dessiné.\nIl regarda, puis : rien.";
  const kindle = "Alors j’ai dessiné. Il regarda, puis : rien.\n￼\n";
  const from = placeAt(0, ios, 0);
  check("a snippet ignores pictures and kinds of space", from.snippet === "Alors j’ai dessiné. Il regarda, puis : rien." && resolvePlace(from, kindle) === 0, from.snippet);
  const repeated = "Oui. Non. Oui. Non. Oui. Non.";
  check("the occurrence nearest the fraction is taken", resolvePlace({ chapter: 0, fraction: 0.5, snippet: "Oui." }, repeated) === 10
    && resolvePlace({ chapter: 0, fraction: 0.95, snippet: "Oui." }, repeated) === 20);
  check("book key normalizes", bookKey("  Le  Grand\tMeaulnes ", "Alain-Fournier") === "le grand meaulnes|alain-fournier");

  check("remote name is author and title", remoteBookName("Vol de nuit", "Saint-Exupéry", []) === "Saint-Exupéry - Vol de nuit.epub");
  check("remote name drops what FAT refuses", remoteBookName('Qui? Quoi: "rien"/ tout.', "", []) === "Qui Quoi rien tout.epub");
  check("remote name avoids taken names, ignoring case", remoteBookName("Nuit", "", ["nuit.epub", "Nuit (2).EPUB"]) === "Nuit (3).epub");
  check("remote name is never empty", remoteBookName("???", "", []) === "Book.epub");
  check("remote name is cut between characters", remoteBookName("é".repeat(200), "", []) === `${"é".repeat(120)}.epub`);
  check("only epubs count as books", isRemoteBook("a.EPUB") && !isRemoteBook("a.pdf") && !isRemoteBook("._a.epub"));
  const samples = [["Vol de nuit", "Saint-Exupéry"], ['Qui? Quoi: "rien"/ tout.', ""], ["...Le Titre.", " Un Auteur "], ["é".repeat(200), ""]];
  check("remote names agree with the browser extension's", samples.every(([title, author]) => remoteBookName(title, author, []) === extensionName(title, author, [])));
}
