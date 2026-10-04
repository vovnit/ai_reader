// Reads the language a book is written in off its prose, by how often the
// commonest words of each language turn up. Coarse, but enough to tell a
// French novel from the "en" its metadata claims.
import { isLetter } from "../../Support/Text.js";

const stopwords = {
  en: "the of and to in is that it was he for on are with as his they at be this have from or had by not but she you were",
  fr: "le la les des et une du que qui dans pas pour ne ce il elle je au sur avec est se son sa ses mais plus vous nous lui",
  de: "der die das und ist nicht ich sie er es ein eine den dem zu mit sich auf für von auch dass wie als aber nach bei war wir",
  es: "el los las del y que en un una es no se por con para su al lo como más pero sus ya este porque muy había era cuando",
  it: "il di che è un una non per in del della con si gli ma come più anche lo se nel sono era questo aveva alla dei",
  pt: "o os as do da dos das e que um uma em não se com para por mais mas como ele ela seu sua foi era muito ao isso",
  ru: "и в не на что он с я как а то все она так его но да ты к у же вы за бы по мне было вот от меня это",
  nl: "het een en van ik te dat die is niet op zijn hij met als voor er maar om ook aan dan was had ze bij naar wat nog",
};
const sets = Object.entries(stopwords).map(([language, words]) => [language, new Set(words.split(" "))]);

/** A few thousand characters from the middle: front matter is often in another language. */
function sample(chapters) {
  let whole = "";
  for (const chapter of chapters) {
    whole += chapter.text + "\n";
    if (whole.length > 200000) break;
  }
  if (whole.length < 200) return "";
  const start = Math.floor(whole.length / 3);
  return whole.slice(start, start + 6000);
}

/** A two-letter code, or "" when no language stands out. */
export function detectLanguage(chapters) {
  const text = sample(chapters).toLowerCase();
  if (!text) return "";
  const hits = new Map();
  let words = 0;
  for (const word of text.split(/[^\p{L}]+/u)) {
    if (!word || !isLetter(word[0])) continue;
    words++;
    for (const [language, set] of sets) if (set.has(word)) hits.set(language, (hits.get(language) ?? 0) + 1);
  }
  if (words < 30) return "";
  let best = "";
  let bestHits = 0;
  let secondHits = 0;
  for (const [language, count] of [...hits].sort(([a], [b]) => (a < b ? -1 : 1))) {
    if (count > bestHits) {
      secondHits = bestHits;
      bestHits = count;
      best = language;
    } else if (count > secondHits) {
      secondHits = count;
    }
  }
  // Stand out clearly, or say nothing.
  if (bestHits * 10 < words || bestHits < Math.floor((secondHits * 3) / 2)) return "";
  return best;
}

const threeLetter = { fre: "fr", fra: "fr", eng: "en", ger: "de", deu: "de", spa: "es", ita: "it", por: "pt", rus: "ru", dut: "nl", nld: "nl" };

/** "fr-FR", "FR" and "fre" all become "fr". */
export function languageCode(declared) {
  const text = declared.trim().toLowerCase().split(/[-_]/)[0];
  return threeLetter[text] ?? text;
}
