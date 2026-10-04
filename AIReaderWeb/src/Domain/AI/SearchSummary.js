// What the model reads back from its searches: the book's passages and the
// web's pages, numbered.
import { cut } from "../../Support/Text.js";

/** How many passages a book search hands the model. */
export const passageLimit = 12;
/** How many pages a web search hands the model, and how much of each. */
export const webResultLimit = 5;
export const webTextLimit = 600;

/** The passages: numbered, each with its book when several are searched, and its chapter. */
export function passagesSummary(query, hits, severalBooks) {
  if (!hits.length) return `No passage read so far contains “${query}”.`;
  let text = `Passages with “${query}”:\n`;
  hits.forEach((hit, index) => {
    text += `${index + 1}. [${severalBooks ? `${hit.bookTitle}, ` : ""}chapter ${hit.chapter + 1}] ${hit.excerpt}\n`;
  });
  return text;
}

function field(item, names) {
  for (const name of names) {
    const value = item[name];
    if (typeof value === "string" && value) return value;
  }
  return "";
}

const isObject = (value) => value !== null && typeof value === "object" && !Array.isArray(value);

/** The first array of objects in `value`, breadth-first, so `{"results": [...]}` is found before anything deeper. */
function firstList(value) {
  if (Array.isArray(value)) return value.length && isObject(value[0]) ? value : null;
  if (!isObject(value)) return null;
  for (const member of Object.values(value)) if (Array.isArray(member) && member.length && isObject(member[0])) return member;
  for (const member of Object.values(value)) {
    const found = firstList(member);
    if (found) return found;
  }
  return null;
}

function shorten(text, limit) {
  return text.length <= limit ? text : `${cut(text, limit)}…`;
}

/** The pages in a search endpoint's answer, whatever the provider, read by the usual field names. */
export function webHits(output) {
  const found = [];
  for (const item of firstList(output) ?? []) {
    const hit = {
      title: field(item, ["title", "name"]),
      url: field(item, ["url", "link", "href"]),
      text: field(item, ["text", "snippet", "description", "content", "summary"]),
    };
    if (!hit.title && !hit.url && !hit.text) continue;
    hit.text = shorten(hit.text.replaceAll("\n", " ").trim(), webTextLimit);
    found.push(hit);
    if (found.length === webResultLimit) break;
  }
  return found;
}

/** The pages, numbered with their address; an unreadable answer is handed over as it came, cut short. */
export function webSummary(query, output) {
  const found = webHits(output);
  if (!found.length) {
    if (output == null || (Array.isArray(output) && !output.length)) return `Nothing was found on the web for “${query}”.`;
    return `The web search for “${query}” answered:\n${shorten(JSON.stringify(output), 3000)}`;
  }
  let text = `Pages found for “${query}”:\n`;
  found.forEach((hit, index) => {
    text += `${index + 1}. ${hit.title || "(untitled)"}${hit.url ? ` — ${hit.url}` : ""}\n`;
    if (hit.text) text += `   ${hit.text}\n`;
  });
  return text;
}
