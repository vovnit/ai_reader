// How a book file's bytes become text, and how the markup scanner reads
// them: the same cases as the Kindle app's check, since the apps must read a
// file alike. TextDecoder, the browser's own, is the reference for UTF-8,
// UTF-16 and the single-byte tables.
import { declaredCharset, decodeBook } from "../../src/Support/Charset.js";
import { decodeEntities, scan } from "../../src/Support/XmlScanner.js";
import { check } from "./Check.mjs";

/** The bytes of each part: a string as UTF-8, an array as it is. */
const bytes = (...parts) => new Uint8Array(parts.flatMap((part) => (typeof part === "string" ? [...new TextEncoder().encode(part)] : part)));
const utf16 = (text, bigEndian) => [...text].flatMap((c) => {
  const units = c.length === 2 ? [c.charCodeAt(0), c.charCodeAt(1)] : [c.charCodeAt(0)];
  return units.flatMap((u) => (bigEndian ? [u >> 8, u & 0xff] : [u & 0xff, u >> 8]));
});
const read = (data) => decodeBook(data, declaredCharset(data));

/** Scanner events on one line: `<name key=value>`, `</name>`, `"text"`. */
function events(markup) {
  const out = [];
  scan(markup, {
    onStart: (name, attributes) => out.push(`<${[name, ...Object.keys(attributes).sort().map((k) => `${k}=${attributes[k]}`)].join(" ")}>`),
    onEnd: (name) => out.push(`</${name}>`),
    onText: (text) => out.push(`"${text}"`),
  });
  return out.join(" ");
}

function checkUnicode() {
  const bom = [0xef, 0xbb, 0xbf];
  const badByte = read(bytes(bom, "Chapter 1\n\nIt", [0x92], "s fine."));
  check("charset: after a UTF-8 mark, a bad byte is one U+FFFD", badByte === "Chapter 1\n\nIt\uFFFDs fine.", JSON.stringify(badByte));
  const subparts = read(bytes(bom, "a", [0xf0, 0x9f, 0x98], "b", [0xed, 0xa0, 0x80], "c", [0xc0, 0xaf], "d", [0xf4, 0x90, 0x80, 0x80], "e", [0xe2, 0x82]));
  check("charset: bad UTF-8 replaced by maximal subparts, as TextDecoder does",
    subparts === "a\uFFFDb\uFFFD\uFFFD\uFFFDc\uFFFD\uFFFDd\uFFFD\uFFFD\uFFFD\uFFFDe\uFFFD", JSON.stringify(subparts));
  const doubled = [read(bytes(bom, bom, "Text")), read(bytes([0xff, 0xfe, 0xff, 0xfe], utf16("Text")))];
  check("charset: a doubled byte-order mark is dropped", doubled.every((text) => text === "Text"), JSON.stringify(doubled));
  const marked = [read(bytes([0xff, 0xfe], utf16("A"), [0x3d, 0xd8], utf16("B😀"), [0x41])), read(bytes([0xfe, 0xff], utf16("Hi", true)))];
  check("charset: UTF-16 after its mark, a pair joined, a lone surrogate and an odd last byte each one U+FFFD",
    marked[0] === "A\uFFFDB😀\uFFFD" && marked[1] === "Hi", JSON.stringify(marked));
  const unmarked = [read(bytes(utf16("Chapter 1\n\nText."))), read(bytes(utf16("Глава 1", true)))];
  check("charset: UTF-16 without a mark, told by its zero bytes, either way round",
    unmarked[0] === "Chapter 1\n\nText." && unmarked[1] === "Глава 1", JSON.stringify(unmarked));
  const sentence = "Это был тихий вечер в Москве.";
  const nul = read(bytes(sentence, [0], sentence));
  check("charset: a stray NUL does not make UTF-8 into UTF-16, and is dropped", nul === sentence + sentence, JSON.stringify(nul));
  const padded = read(bytes("Chapter 1\n\nIt was a fine day.\n", new Array(27).fill(0)));
  check("charset: UTF-8 padded out with NULs is not UTF-16, and the NULs are dropped", padded === "Chapter 1\n\nIt was a fine day.\n", JSON.stringify(padded));
  const nonchars = [read(bytes("A", [0xef, 0xbf, 0xbe], "B", [0xef, 0xbf, 0xbf], "C")), read(bytes([0xff, 0xfe], utf16("A"), [0xfe, 0xff], utf16("B")))];
  check("charset: the non-characters U+FFFE and U+FFFF, which XML forbids, are dropped",
    nonchars[0] === "ABC" && nonchars[1] === "AB", JSON.stringify(nonchars));
}

function checkSingleByte() {
  const wrong = '<?xml version="1.0" encoding="windows-1251"?><p>Война</p>';
  const stray = read(bytes("Глава 1\n\nТихий вечер ", [0x97], " в Москве."));
  check("charset: valid UTF-8 wins over a wrong declaration, and keeps its text past a stray byte",
    read(bytes(wrong)) === wrong && stray === "Глава 1\n\nТихий вечер \uFFFD в Москве.", JSON.stringify(stray));
  const ascii = read(bytes("It", [0x92], "s"));
  check("charset: ASCII with one stray byte is windows-1252, not a U+FFFD", ascii === "It\u2019s", JSON.stringify(ascii));
  const declared = [
    read(bytes('<?xml encoding="windows-1251"?>', [0xc2, 0xee, 0xe9, 0xed, 0xe0])),
    read(bytes('<?xml encoding="koi8-r"?>', [0xe7, 0xcc, 0xc1, 0xd7, 0xc1])),
    read(bytes('<meta charset="iso-8859-1">', [0x80], " Tr", [0xe4], "ume")),
  ];
  check("charset: a declared windows-1251, koi8-r or windows-1252",
    declared.join(" | ") === '<?xml encoding="windows-1251"?>Война | <?xml encoding="koi8-r"?>Глава | <meta charset="iso-8859-1">€ Träume',
    declared.join(" | "));
  const war = [0xc2, 0xee, 0xe9, 0xed, 0xe0, 0x20, 0xe8, 0x20, 0xec, 0xe8, 0xf0];
  const cover = read(bytes("<FictionBook><book-title>", war, "</book-title><binary>", "QUJDREVGR0hJSktM".repeat(20), "</binary></FictionBook>"));
  const latinHeavy = read(bytes("Chapter 1\n\nThe word ", [0xab, 0xe2, 0xe5, 0xf7, 0xe5, 0xf0, 0xbb], " means evening in Russian."));
  check("charset: nothing declared, runs of high bytes are windows-1251, markup and base64 aside",
    cover.includes("<book-title>Война и мир</book-title>") && latinHeavy === "Chapter 1\n\nThe word «вечер» means evening in Russian.",
    JSON.stringify(latinHeavy));
  const western = read(bytes("Tr", [0xe4], "ume und Caf", [0xe9]));
  check("charset: nothing declared, accents inside Latin words are windows-1252", western === "Träume und Café", western);
  const koi8 = [0xe7, 0xcc, 0xc1, 0xd7, 0xc1, 0x20, 0x31, 0x0d, 0x0a, 0x0d, 0x0a, 0xf0, 0xc5, 0xd4, 0xd2, 0x20, 0xe1, 0xce, 0xc4, 0xd2, 0xc5, 0xc5, 0xd7, 0xc9,
    0xde, 0x20, 0xd3, 0xcc, 0xd5, 0xd6, 0xc9, 0xcc, 0x2e];
  const lowerCase = [read(bytes(koi8)), read(bytes('<?xml encoding="koi8-u"?>', [0xe7, 0xcc, 0xc1, 0xd7, 0xc1]))];
  check("charset: nothing declared, mostly 0xC0-0xDF is KOI8-R, where а-я are; a declared koi8-u reads as koi8-r",
    lowerCase[0] === "Глава 1\r\n\r\nПетр Андреевич служил." && lowerCase[1] === '<?xml encoding="koi8-u"?>Глава', JSON.stringify(lowerCase));
  const controls = read(bytes("a", [0x0c], "b", [0x0b], "c", [0x01], "d", [0x1a], "e", [0x7f], "f\tg\r\n", [0x1f]));
  check("charset: a form feed or vertical tab ends a line; the other controls XML forbids are dropped",
    controls === "a\nb\ncdef\tg\r\n", JSON.stringify(controls));
  const names = [
    declaredCharset(bytes('<?XML version="1.0" ENCODING = " Windows-1251\n"?>')),
    declaredCharset(bytes("<META CHARSET=KOI8-R>")),
    declaredCharset(bytes('<meta http-equiv="Content-Type" content="text/html; charset=windows-1252">')),
    declaredCharset(bytes("<p>none</p>")),
  ];
  check("charset: a declared name lowercased and trimmed of spaces and line breaks; none is empty",
    names.join("|") === "windows-1251|koi8-r|windows-1252|", names.join("|"));
}

/** Against TextDecoder itself, which the C++ and Swift ports cannot call. */
function checkAgainstTextDecoder() {
  const high = Uint8Array.from({ length: 128 }, (_, i) => 0x80 + i);
  // Node's windows-1252 is Latin-1, unlike a browser's, so that table is checked by its cases above.
  check("charset: the windows-1251 and koi8-r tables are TextDecoder's",
    ["windows-1251", "koi8-r"].every((name) => decodeBook(high, name) === new TextDecoder(name).decode(high)));
  // What the EPUB cannot carry goes, then any byte-order mark left at the start.
  const clean = (text) => text.replace(/[\v\f]/g, "\n").replace(/[\0-\x08\x0e-\x1f\x7f\uFFFE\uFFFF]/g, "").replace(/^\uFEFF+/, "");
  const marks = { "utf-8": [0xef, 0xbb, 0xbf], "utf-16le": [0xff, 0xfe], "utf-16be": [0xfe, 0xff] };
  const pool = [0x00, 0x0c, 0x41, 0x80, 0x8f, 0x90, 0x9f, 0xa0, 0xbb, 0xbf, 0xc2, 0xd8, 0xdb, 0xdc, 0xdf, 0xe0, 0xed, 0xef, 0xf0, 0xf4, 0xf5, 0xfe, 0xff];
  let seed = 1;
  const random = (n) => (seed = (seed * 1103515245 + 12345) >>> 0) % n;
  const differing = [];
  for (let i = 0; i < 3000; i++) {
    const body = Array.from({ length: random(24) }, () => (i % 2 ? pool[random(pool.length)] : random(256)));
    for (const [label, mark] of Object.entries(marks)) {
      if (decodeBook(bytes(mark, body)) !== clean(new TextDecoder(label, { ignoreBOM: true }).decode(bytes(body)))) differing.push(`${label} ${body}`);
    }
  }
  check("charset: random bytes after each mark decode as TextDecoder decodes them", differing.length === 0, differing.slice(0, 3).join("; "));
}

function checkScanner() {
  check("scanner: a '<' that opens no tag is text", events("<p>if 1 < 2 then x</p><p>y</p>x<") === '<p> "if 1 < 2 then x" </p> <p> "y" </p> "x<"',
    events("<p>if 1 < 2 then x</p><p>y</p>x<"));
  check("scanner: an empty end tag is skipped", events("<p>a</>b</p>") === '<p> "a" "b" </p>', events("<p>a</>b</p>"));
  const names = events("<DIV CLASS=X Xml:Lang='TR'><Iİ>t</Iİ><ns:P/></DIV><İ>");
  check("scanner: names lowercased by ASCII alone, without their namespace; '<' before a letter not ASCII is text",
    names === '<div class=X lang=TR> <iİ> "t" </iİ> <p> </p> </div> "<İ>"', names);
  const references = decodeEntities("&amp;&lt;&#65;&#x42;&#X43;&#0065;&#-1;&#+65;&# 65;&#x0x41;&#65&#66;&#xD83D;&#x110000;&#0;"
    + "&unknown;&abcdefghij;&#x1F600;&shy;&nbsp;");
  check("scanner: references are '&', 1-9 letters, digits or '#', ';'; a number not a scalar value is dropped",
    references === "&<ABCA&#-1;&#+65;&# 65;&#65B&unknown;&abcdefghij;😀\u00A0", JSON.stringify(references));
  const forbidden = decodeEntities("a&#1;&#x8;&#11;&#12;&#x0E;&#31;&#xFFFE;&#xFFFF;b&#9;&#10;&#13;&#x20;&#xD7FF;&#xE000;&#xFFFD;&#x10000;&#x7F;c");
  check("scanner: a reference to a character XML forbids is dropped",
    forbidden === "ab\t\n\r \uD7FF\uE000\uFFFD\u{10000}\x7Fc", JSON.stringify(forbidden));
}

export function checkCharset() {
  checkUnicode();
  checkSingleByte();
  checkAgainstTextDecoder();
  checkScanner();
}
