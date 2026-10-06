// What `expand_context` reads: the text just before the passage a
// conversation is about — the sentence of a lookup, the page of a chat — or
// just after it. Each call reads one step further, within the chapter.
import { argument } from "./Tools.js";
import { stepBack, stepForward } from "../Search/BookSearch.js";

/** What the model is told when there is no passage to read around, or it asked for neither direction. */
export const nothingAround = "There is no book text around this to read.";
export const unknownDirection = "The direction must be “before” or “after”.";

/** "before" or "after", or null when the model sent something else. */
export function contextDirection(args) {
  const value = argument(args, "direction");
  return value === "before" || value === "after" ? value : null;
}

/**
 * Reads one step past `window` — `{ start, end }` in `chapter`, the text of
 * its chapter — widens the window by it, and says what was read as the
 * model reads it.
 */
export function readAround(direction, chapter, window) {
  window.start = Math.min(Math.max(window.start, 0), chapter.length);
  window.end = Math.min(Math.max(window.end, window.start), chapter.length);
  if (direction === "before") {
    if (window.start === 0) return "Nothing comes before it: the chapter begins there.";
    const from = stepBack(chapter, window.start);
    const text = chapter.slice(from, window.start).trim();
    window.start = from;
    return `Before it in the book:\n${text}${from === 0 ? "\n(The chapter begins here.)" : ""}`;
  }
  if (window.end === chapter.length) return "Nothing comes after it: the chapter ends there.";
  const to = stepForward(chapter, window.end);
  const text = chapter.slice(window.end, to).trim();
  window.end = to;
  return `After it in the book:\n${text}${to === chapter.length ? "\n(The chapter ends here.)" : ""}`;
}
