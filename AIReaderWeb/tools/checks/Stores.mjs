// The stores over the database, the settings and the dictionaries — the
// Kindle check's cases, plus the bundled dictionary read by the web app's
// own SQLite reader.
import { gzipSync } from "node:zlib";
import { stripDsl } from "../../src/Domain/Formats/DSLDictionaryReader.js";
import { dictionarySources } from "../../src/Domain/Formats/DictionaryFormat.js";
import { decode } from "../../src/Support/TextFile.js";
import { packLanguages } from "../../src/Services/DictionaryPacks.js";
import { SettingsStore, aiToken, defaultWeb } from "../../src/Services/Settings.js";
import { MemoryStorage } from "../MemoryDatabase.js";
import { check } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";

const utf16le = (text) => new Blob([new Uint8Array([0xff, 0xfe]), new Uint16Array([...text].map((c) => c.charCodeAt(0)))]);
const bigEndian = (value) => new Uint8Array([value >>> 24, (value >>> 16) & 255, (value >>> 8) & 255, value & 255]);
const file = (name, parts) => new File(Array.isArray(parts) ? parts : [parts], name);

export async function checkStores() {
  const env = await freshEnv();
  const id = await env.library.add({ title: "Test", author: "", language: "" }, new Blob(["x"]), null);
  await env.library.savePosition(id, 2, 345, null);
  const stored = await env.library.find(id);
  check("library stores books and positions", stored.readingChapter === 2 && stored.readingOffset === 345);

  const context = { word: "maisons", sentence: "Les maisons.", language: "fr", bookId: id };
  check("cache misses first", !(await env.lookups.cached(context)));
  await env.lookups.save(context, { lemma: "maison", formNote: "pl.", meaning: "дом", guessed: false, confidence: 0.9 });
  check("cache hits after save", (await env.lookups.cached(context))?.lemma === "maison");
  await env.lookups.save(context, { lemma: "maison", formNote: "pl.", meaning: "здание", guessed: false, confidence: 0.8 });
  check("saving again replaces", (await env.lookups.all()).length === 1 && (await env.lookups.all(id))[0].meaning === "здание");
  let cards = await env.cards.all(id);
  check("every lookup is a card", cards.length === 1 && cards[0].front === "maisons" && cards[0].back === "здание" && cards[0].example === "Les ____." && cards[0].practicedAt === "");
  await env.cards.record(cards[0].lookupId, false);
  await env.cards.record(cards[0].lookupId, true);
  cards = await env.cards.all();
  check("practice is counted against the card", cards.length === 1 && cards[0].correct === 1 && cards[0].wrong === 1 && cards[0].practicedAt !== "");
  check("cards of another book are not listed", !(await env.cards.all(id + 1)).length);
  await env.lookups.remove((await env.lookups.all())[0].id);
  check("removing a lookup takes its practice with it", !(await env.lookups.all()).length && !(await env.cards.all()).length);

  const series = await env.groups.named("Série");
  check("a group is made once by name", series > 0 && (await env.groups.named("Série")) === series && (await env.groups.all()).length === 1);
  await env.library.assignGroup(id, series);
  check("a book joins a group", (await env.library.find(id)).groupId === series && (await env.library.inGroup(series)).length === 1);
  await env.groups.remove(series);
  check("a dissolved group leaves its books", !(await env.groups.all()).length && (await env.library.find(id)).groupId === 0);
  await env.lookups.save(context, { lemma: "maison", formNote: "", meaning: "дом", guessed: false, confidence: 0.9 });
  await env.library.remove(id);
  check("a removed book's words are kept, belonging to none", (await env.lookups.all())[0].bookId === null && !(await env.library.file(id)));

  const packs = await env.packs.all();
  check("bundled pack is listed", packs.length === 1 && packs[0].kind === "bundled" && packs[0].isEnabled && packLanguages(packs[0]) === "fr → ru");
  await env.packs.setEnabled(packs[0].id, false);
  check("packs can be disabled", !(await env.packs.enabled()).length);
  await env.packs.setEnabled(packs[0].id, true);
  const avait = await env.dictionary.lookup("Avait,", await env.packs.enabled());
  check("the bundled dictionary is read: a form and its lemma's articles", avait.query === "avait" && avait.forms.some((form) => form.lemma === "avoir" && form.features.includes("ind:imp:3"))
    && avait.articles.some((article) => article.lemma === "avoir" && article.senses.length > 0), JSON.stringify(avait).slice(0, 300));
  check("an entry is the lemma's own articles", (await env.dictionary.articlesFor("avoir", packs)).every((article) => article.lemma === "avoir")
    && (await env.dictionary.articlesFor("avoir", packs)).length > 0);

  const storage = new MemoryStorage();
  const settings = new SettingsStore(storage);
  settings.saveAi({ endpoint: "mock://ai", apiKey: "t1", openAIKey: "t2", model: "mock-medium", language: "" });
  check("answer language defaults to Russian when cleared", settings.ai().language === "Russian");
  settings.saveAi({ ...settings.ai(), language: "English" });
  check("settings round-trip", settings.ai().language === "English" && settings.ai().apiKey === "t1");
  check("token follows the endpoint", aiToken(settings.ai()) === "t1" && aiToken({ endpoint: "https://api.openai.com/v1", apiKey: "t1", openAIKey: "t2" }) === "t2");
  check("no token ships with the app", new SettingsStore(new MemoryStorage()).ai().apiKey === "");
  settings.saveWeb({ ...defaultWeb, apiKey: "monid_x", input: '{"q": "$query"}' });
  check("settings keep the web search", settings.web().apiKey === "monid_x" && settings.web().input === '{"q": "$query"}');
  settings.saveStyle({ scale: 1.7, fontName: "Georgia", lineSpacing: 2, margin: 24 });
  check("style round-trip", settings.style().scale === 1.7 && settings.style().fontName === "Georgia");
  const run = new SettingsStore(storage, SettingsStore.runValues("?aiEndpoint=https%3A%2F%2Fexample%2Fv1"));
  run.saveAi({ ...run.ai(), language: "French" });
  check("run overrides win without saving", run.ai().endpoint === "https://example/v1" && settings.ai().endpoint === "mock://ai" && settings.ai().language === "French");

  const words = await env.packs.addFiles([file("mots.tsv", "# mot\tsens\nchat\tкот\tкошка\nmaison\tдом\n")]);
  check("word list converts into a pack", words.added === 1 && !words.errors.length, words.errors.join());
  check("adding it again adds nothing", (await env.packs.addFiles([file("mots.tsv", "chat\tкот\n")])).added === 0);
  const wordList = (await env.packs.all()).filter((pack) => pack.kind === "converted");
  const chat = await env.dictionary.lookup("Chat,", wordList);
  check("converted pack answers lookups", chat.articles.length === 1 && chat.articles[0].senses.join() === "кот,кошка", JSON.stringify(chat));

  const article = "h<b>кот</b><br/>кошка";
  const bytes = new TextEncoder().encode(article);
  const dsl = file("lingvo.dsl", utf16le("#NAME \"Petit Lingvo\"\n#INDEX_LANGUAGE \"French\"\n#CONTENTS_LANGUAGE \"Russian\"\n\n"
    + "pomme\n\t[m1][p]f[/p] яблоко[/m]\n\t[m1]{{comment}}[i]pomme de terre[/i] картофель[/m]\npoire\npoirier\n\t[m1]груша \\[плод\\][/m]\n"));
  const star = [
    file("star.ifo", "StarDict's dict ifo file\nversion=2.4.2\nbookname=Star FR\nwordcount=1\nlang=fr\ntargetlang=ru\n"),
    file("star.idx", [new TextEncoder().encode("chat"), new Uint8Array([0]), bigEndian(0), bigEndian(bytes.length)]),
    file("star.dict.dz", gzipSync(bytes)),
  ];
  const xdxf = file("open.xdxf", '<?xml version="1.0"?><xdxf lang_from="fra" lang_to="rus"><full_name>Open FR</full_name><ar><k>chien</k>\nсобака\nпёс</ar><ar><k>loup</k><k>louve</k>\nволк</ar></xdxf>');
  const sources = await dictionarySources([dsl, ...star, xdxf]);
  check("formats are recognized and StarDict files grouped", sources.length === 3 && sources.find((source) => source.format === "stardict").companions.length === 2);
  check("dsl markup strips", stripDsl("[m1]{{x}}[i]pomme[/i] \\[plod\\][/m]") === "pomme [plod]");
  check("utf-16 files decode", decode(new Uint8Array(await utf16le("été").arrayBuffer())) === "été");
  const converted = await env.packs.addFiles([dsl, ...star, xdxf, file("notes.pdf", "not a dictionary")]);
  check("dsl, stardict and xdxf convert; a stranger is named", converted.added === 3 && converted.errors.length === 1 && converted.errors[0].startsWith("notes.pdf"), converted.errors.join(" / "));
  const named = (await env.packs.all()).filter((pack) => pack.kind === "converted").map((pack) => `${pack.name} (${packLanguages(pack)})`).join("; ");
  check("converted packs carry their names and languages", named.includes("Petit Lingvo (fr → ru)") && named.includes("Star FR (fr → ru)") && named.includes("Open FR (fr → ru)"), named);
  const all = (await env.packs.all()).filter((pack) => pack.kind === "converted");
  const pomme = await env.dictionary.lookup("Pomme", all);
  check("dsl article", pomme.articles.length === 1 && pomme.articles[0].senses.length === 2 && pomme.articles[0].senses[0] === "f яблоко", JSON.stringify(pomme.articles));
  check("dsl shared article", (await env.dictionary.lookup("poirier", all)).articles[0]?.senses[0] === "груша [плод]");
  const starChat = await env.dictionary.lookup("chat", all.filter((pack) => pack.name === "Star FR"));
  check("stardict senses through gzip and html", starChat.articles[0]?.senses.join() === "кот,кошка", JSON.stringify(starChat.articles));
  check("xdxf second headword", (await env.dictionary.lookup("louve", all)).articles[0]?.senses[0] === "волк");
  const lone = await env.packs.addFiles([file("saule.ifo", "bookname=Saule\n")]);
  check("a StarDict .ifo alone is refused with a reason", lone.added === 0 && lone.errors[0]?.includes(".idx"), lone.errors.join());
  const lingvo = (await env.packs.all()).find((pack) => pack.name === "Petit Lingvo");
  await env.packs.remove(lingvo);
  check("removing a converted pack removes its articles", !(await env.dictionary.lookup("pomme", [lingvo])).articles.length);
}
