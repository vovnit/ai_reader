// The photo lookup below the view: the finger found in a drawn page, the
// word it points at and its sentence, Tesseract's page as lines, and the
// feature reading photos through a stand-in reader against the mock.
import { findFinger } from "../../src/Domain/Photo/FingerFinder.js";
import { photoPassage } from "../../src/Domain/Photo/PhotoPassage.js";
import { photoWords } from "../../src/Domain/Photo/PhotoWord.js";
import { pointedWord } from "../../src/Domain/Photo/PointedWord.js";
import { tesseractLines } from "../../src/Domain/Photo/TesseractPage.js";
import { PhotoFeature } from "../../src/Features/Photo/PhotoFeature.js";
import { defaultAi, mockEndpoint } from "../../src/Services/Settings.js";
import { RgbImage } from "../../src/Support/RgbImage.js";
import { check } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";

const paper = [238, 234, 225];
const ink = [45, 42, 40];
const skin = [205, 150, 120];

/** Lines of words in photo pixels: `texts` laid out from (x, y), `height` tall, `gap` apart. */
function layout(texts, { x = 60, y = 120, height = 24, gap = 16, letter = 11, space = 14 } = {}) {
  return texts.map((text, row) => {
    const top = y + row * (height + gap);
    const words = [];
    let left = x;
    for (const match of text.matchAll(/\S+/g)) {
      const width = match[0].length * letter;
      words.push({ location: match.index, length: match[0].length, box: { x: left, y: top, width, height } });
      left += width + space;
    }
    return { text, words, height, up: { dx: 0, dy: -1 } };
  });
}

/** A photo of the page: paper, print in the word boxes, and a finger from the bottom edge whose round tip ends at `tip`. */
function drawn(width, height, lines, finger = null) {
  const values = new Float32Array(width * height * 3);
  const paint = (x, y, colour) => values.set(colour, 3 * (y * width + x));
  for (let i = 0; i < width * height; i++) values.set(paper, 3 * i);
  let seed = 7;
  const random = () => (seed = (seed * 1103515245 + 12345) % 2147483648) / 2147483648;
  for (const word of lines.flatMap((line) => line.words)) {
    // Letters of uneven width and height, as print has: texture the finger lacks.
    for (let x = word.box.x; x < word.box.x + word.box.width;) {
      const stroke = 3 + Math.floor(random() * 4);
      const top = word.box.y + (random() < 0.3 ? 0 : 8);
      for (let dx = 0; dx < stroke; dx++) for (let y = top; y < word.box.y + word.box.height - (random() < 0.2 ? 0 : 4); y++) paint(x + dx, y, ink);
      x += stroke + 3 + Math.floor(random() * 4);
    }
  }
  if (finger) {
    const radius = finger.width / 2;
    for (let y = finger.tip.y; y < height; y++) {
      for (let x = Math.floor(finger.tip.x - radius); x <= finger.tip.x + radius; x++) {
        const below = y - (finger.tip.y + radius);
        if (below < 0 && (x - finger.tip.x) ** 2 + below ** 2 > radius * radius) continue;
        const shade = 1 - 0.1 * Math.abs(x - finger.tip.x) / radius;
        paint(x, y, skin.map((value) => value * shade));
      }
    }
  }
  return new RgbImage(width, height, values);
}

const wordAt = (photo, place) => {
  const line = photo.lines[place.line];
  const word = line.words[place.word];
  return line.text.slice(word.location, word.location + word.length);
};

function checkDomain() {
  const texts = [
    "Il était une fois une vieille maison au bord",
    "de la forêt, et dans cette maison vivait une",
    "femme avec son chat. Le soir, elle lisait près",
    "du feu de grands livres de cuisine pleins de",
    "recettes de lentilles et de soupes d'autrefois.",
  ];
  const lines = layout(texts);
  const target = lines[4].words[2];
  const tip = { x: target.box.x + target.box.width / 2, y: target.box.y + target.box.height + 6 };
  const photo = { lines, fingers: [{ tip, direction: { dx: 0, dy: -1 } }] };
  const place = pointedWord(photo);
  check("a finger below a word points at it", place && wordAt(photo, place) === "lentilles", JSON.stringify(place));
  check("nothing is pointed at from behind the text", !pointedWord({ lines, fingers: [{ tip: { x: tip.x, y: 60 }, direction: { dx: 0, dy: -1 } }] }));
  check("nothing is pointed at without a finger", !pointedWord({ lines, fingers: [] }));

  const passage = photoPassage(photo);
  check("a photo's lines read as one passage", passage.text.startsWith("Il était une fois") && passage.text.includes("au bord de la forêt") && !passage.text.includes("\n"), passage.text);
  check("the passage knows where the pointed word starts", passage.text.slice(passage.pointedOffset).startsWith("lentilles "), String(passage.pointedOffset));
  const hyphenated = photoPassage({ lines: layout(["une mai-", "son au bord"]), fingers: [] });
  check("a word broken at a line's end is mended", hyphenated.text === "une maison au bord" && hyphenated.pointedOffset === null, hyphenated.text);
  check("each word read starts where the passage has it", passage.words.length === lines.flatMap((line) => line.words).length
    && passage.words.every((word) => passage.text.startsWith(word.text, word.offset)), JSON.stringify(passage.words.slice(0, 3)));
  const [, broken, rest] = hyphenated.words;
  check("a broken word shows as printed and is taken whole", broken.text === "mai-" && hyphenated.text.startsWith("maison", broken.offset)
    && hyphenated.text.startsWith("son", rest.offset), JSON.stringify(hyphenated.words));

  // A word 90 long on a line 20 tall, its upright box drawn around it turned with the page.
  for (const degrees of [0, 20, -35, 90, 180]) {
    const angle = (degrees * Math.PI) / 180;
    const cos = Math.cos(angle);
    const sin = Math.sin(angle);
    const width = 90 * Math.abs(cos) + 20 * Math.abs(sin);
    const height = 90 * Math.abs(sin) + 20 * Math.abs(cos);
    const box = { x: 300 - width / 2, y: 200 - height / 2, width, height };
    const [word] = photoWords({ text: "maison", words: [{ location: 0, length: 6, box }], height: 20, up: { dx: sin, dy: -cos } }, 10);
    check(`a word on a line turned ${degrees}° is placed along it`, word.text === "maison" && word.offset === 10
      && Math.abs(word.size.width - 90) < 1e-9 && Math.abs(word.size.height - 20) < 1e-9 && Math.hypot(word.center.x - 300, word.center.y - 200) < 1e-9
      && Math.abs(Math.cos(word.angle) - cos) < 1e-9 && Math.abs(Math.sin(word.angle) - sin) < 1e-9, JSON.stringify(word));
  }
  const uneven = photoWords({ text: "le ma gy", up: { dx: 0, dy: -1 }, height: 26, words: [
    { location: 0, length: 2, box: { x: 0, y: 10, width: 20, height: 20 } },
    { location: 3, length: 2, box: { x: 30, y: 16, width: 24, height: 14 } },
    { location: 6, length: 2, box: { x: 60, y: 16, width: 20, height: 20 } },
  ] }, 0);
  check("a line's words sit level, covering its tallest letter and its lowest", uneven.every((word) => word.center.y === 23 && word.size.height === 26)
    && uneven.map((word) => word.center.x).join() === "10,42,70", JSON.stringify(uneven));

  const size = { width: 1024, height: 768 };
  const finger = { tip, width: 84 };
  const found = findFinger(drawn(size.width, size.height, lines, finger).shrunk(256), lines, size);
  check("a finger is found in a drawn photo", found && Math.hypot(found.tip.x - tip.x, found.tip.y - tip.y) < 24 && found.direction.dy < -0.7, JSON.stringify(found));
  const seen = found && pointedWord({ lines, fingers: [found] });
  check("and points at the word above it", seen && wordAt(photo, seen) === "lentilles", JSON.stringify(seen));
  check("no finger is made up on a page without one", !findFinger(drawn(size.width, size.height, lines).shrunk(256), lines, size));

  const word = (text, x0, y0, x1, y1, confidence = 90) => ({ text, confidence, bbox: { x0, y0, x1, y1 } });
  const blocks = [{ paragraphs: [{ lines: [
    { baseline: { x0: 10, y0: 40, x1: 210, y1: 30, has_baseline: true }, words: [word("Le", 10, 20, 40, 40), word("~", 45, 22, 50, 40, 12), word("soir,", 60, 18, 120, 42), word("été", 130, 15, 170, 41)] },
    { baseline: { x0: 0, y0: 0, x1: 0, y1: 0, has_baseline: false }, words: [word("", 10, 60, 20, 80), word("|", 30, 60, 32, 80, 5)] },
  ] }] }];
  const read = tesseractLines(blocks);
  const [first] = read;
  check("Tesseract's lines keep their words and drop the doubtful", read.length === 1 && first.text === "Le soir, été" && first.words.length === 3
    && first.words[2].location === 9 && first.words[2].length === 3 && first.words[1].box.x === 60 && first.words[1].box.height === 24, JSON.stringify(read));
  check("a line's top faces away from its baseline", first.up.dy < -0.99 && first.up.dx < 0 && Math.abs(Math.hypot(first.up.dx, first.up.dy) - 1) < 1e-9, JSON.stringify(first.up));

  // Print 20 tall on a page turned 10°: Tesseract's upright boxes make the line taller than that.
  const [cos, sin] = [Math.cos(Math.PI / 18), Math.sin(Math.PI / 18)];
  const lengths = [30, 50, 70, 40];
  let run = 0;
  const turnedWords = ["le", "chat", "dormait", "bien"].map((text, index) => {
    const middle = run + lengths[index] / 2;
    run += lengths[index] + 12;
    const [x, y] = [100 + middle * cos, 300 + middle * sin];
    const [halfWidth, halfHeight] = [(lengths[index] * cos + 20 * sin) / 2, (lengths[index] * sin + 20 * cos) / 2];
    return word(text, x - halfWidth, y - halfHeight, x + halfWidth, y + halfHeight);
  });
  const turnedLines = tesseractLines([{ paragraphs: [{ lines: [{ baseline: { x0: 100, y0: 300, x1: 100 + 200 * cos, y1: 300 + 200 * sin }, words: turnedWords }] }] }]);
  const turned = photoPassage({ lines: turnedLines, fingers: [], size: { width: 600, height: 600 } });
  check("a turned page's words are as tall as its print, not its boxes", turnedLines[0].height > 25
    && turned.words.every((placed, index) => Math.abs(placed.size.height - 20) < 1e-9 && Math.abs(placed.size.width - lengths[index]) < 1e-9),
  JSON.stringify(turned.words.map((placed) => placed.size)));

  const quotedLines = tesseractLines([{ paragraphs: [{ lines: [{ baseline: { x0: 10, y0: 40, x1: 260, y1: 40 },
    words: [word("—¿Qué", 10, 20, 70, 40), word("dices?", 80, 20, 140, 40), word("«Bonjour»,", 150, 20, 250, 40)] }] }] }]);
  const quoted = photoPassage({ lines: quotedLines, fingers: [{ tip: { x: 200, y: 46 }, direction: { dx: 0, dy: -1 } }], size: { width: 300, height: 100 } });
  check("a word read with a quote or a dash before it starts at its first letter", quoted.words.map((placed) => placed.text).join(" ") === "—¿Qué dices? «Bonjour»,"
    && ["Qué", "dices", "Bonjour"].every((spelling, index) => quoted.text.startsWith(spelling, quoted.words[index].offset))
    && quoted.text.startsWith("Bonjour", quoted.pointedOffset), JSON.stringify(quoted));
}

class StandInReader {
  asked = [];
  closed = false;
  /** `offline` is a language whose model cannot be fetched. */
  constructor(answer, offline = "") {
    this.answer = answer;
    this.offline = offline;
  }
  async read(file, language, onStatus) {
    this.asked.push(language);
    onStatus("reading");
    if (this.answer instanceof Error) throw this.answer;
    if (language === this.offline) throw new Error("Failed to fetch");
    return this.answer;
  }
  close() {
    this.closed = true;
  }
}

/** Waits for the lookup's answer, or its failure. */
const settled = (lookup) => new Promise((resolve) => {
  const done = () => lookup.explanation || lookup.error;
  if (done()) return resolve();
  const stop = lookup.subscribe(() => done() && (stop(), resolve()));
});

async function checkFeature() {
  const lines = layout([
    "Il était une fois une vieille maison au bord de la forêt, et dans",
    "cette maison vivait une femme avec son chat. Le soir, elle lisait",
    "près du feu de grands livres de cuisine pleins de recettes et de",
    "soupes d'autrefois, que sa mère lui avait laissés dans la maison.",
  ], { letter: 9 });
  const target = lines[1].words[1];
  const fingers = [{ tip: { x: target.box.x + target.box.width / 2, y: target.box.y + target.box.height + 4 }, direction: { dx: 0, dy: -1 } }];
  const env = await freshEnv();
  env.settings.saveAi({ ...defaultAi, endpoint: mockEndpoint, model: "mock-medium" });
  const reader = new StandInReader({ lines, fingers });
  const feature = new PhotoFeature(env, reader);
  const statuses = [];
  feature.subscribe(() => statuses.push(feature.status));
  await feature.read(new Blob(["photo"]));
  check("a page in another language is read again in its own", reader.asked.join() === "en,fr" && env.settings.photoLanguage() === "fr", reader.asked.join());
  check("the pointed word is looked up in its sentence", feature.lookup?.context.word === "maison" && feature.lookup.context.sentence.startsWith("Il était une fois")
    && feature.lookup.context.sentence.endsWith("son chat.") && feature.lookup.context.language === "fr" && !feature.error, JSON.stringify(feature.lookup?.context));
  check("the reader is told the photo is being read", statuses[0] === "reading" && feature.status === "");
  await settled(feature.lookup);
  const [saved] = await env.lookups.all();
  check("the mock explains it, filed under no book", feature.lookup.explanation?.meaning && saved?.word === "maison" && saved.bookId === null, feature.lookup.error);

  reader.asked = [];
  await feature.read(new Blob(["photo"]));
  check("the next photo is read in the language the last was in", reader.asked.join() === "fr", reader.asked.join());

  reader.asked = [];
  feature.read(new Blob(["a"]));
  feature.read(new Blob(["b"]));
  await feature.read(new Blob(["c"]));
  check("photos taken while one is read leave only the last to read", reader.asked.length === 1);

  reader.answer = { lines, fingers: [], size: { width: 1024, height: 768 } };
  const page = new Blob(["photo"]);
  await feature.read(page);
  check("without a finger there is the text and no lookup", feature.passage?.text && !feature.lookup && !feature.error);
  check("and the photo, to show its words over", feature.photo === page && feature.passage.photoSize.width === 1024
    && feature.passage.words.length === lines.flatMap((line) => line.words).length);
  const tapped = feature.wordAt(feature.passage.text.indexOf("chat") + 1);
  check("a word tapped in that text is taken with its sentence", tapped?.word === "chat" && tapped.sentence.endsWith("son chat.") && tapped.language === "fr", JSON.stringify(tapped));
  check("a tap between words is no word", feature.wordAt(feature.passage.text.indexOf(" chat")) === null);
  reader.answer = { lines: [], fingers: [] };
  await feature.read(new Blob(["photo"]));
  check("a photo without text says so", feature.error === "No text was found in the photo." && !feature.passage);
  reader.answer = new Error("The photo couldn’t be opened.");
  await feature.read(new Blob(["photo"]));
  check("a photo that cannot be read says why", feature.error === "The photo couldn’t be opened." && feature.status === "");
  feature.close();
  check("closing frees the reader", reader.closed);

  const offline = await freshEnv();
  offline.settings.saveAi({ ...defaultAi, endpoint: mockEndpoint, model: "mock-medium" });
  const unfetched = new PhotoFeature(offline, new StandInReader({ lines, fingers }, "fr"));
  await unfetched.read(new Blob(["photo"]));
  check("without the right model the first reading still explains the word", unfetched.lookup?.context.word === "maison"
    && unfetched.lookup.context.language === "fr" && !unfetched.error, unfetched.error);
  unfetched.close();

  const other = await freshEnv();
  await other.library.add({ title: "Roman", author: "", language: "de-DE" }, new Blob(["x"]), null);
  const shelf = new StandInReader({ lines: [], fingers: [] });
  await new PhotoFeature(other, shelf).read(new Blob(["photo"]));
  check("the first photo is read in the language of the shelf's book", shelf.asked.join() === "de", shelf.asked.join());
}

export async function checkPhoto() {
  checkDomain();
  await checkFeature();
}
