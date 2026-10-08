// Remembers which adjustments each endpoint and model needed
// (`Domain/AI/RequestQuirks.js`), so finding them out — a refused request
// each — is paid once, not again on every page load. Kept in `localStorage`
// once `keepQuirksIn` is given it; until then, for this page only.

const key = "requestQuirks";
let storage = null;
let known = new Map();

/** Reads what earlier loads learned from `place` (`localStorage` or alike), and writes there from now on. */
export function keepQuirksIn(place) {
  storage = place;
  known = new Map();
  try {
    for (const [signature, quirks] of Object.entries(JSON.parse(place.getItem(key) ?? "{}"))) known.set(signature, new Set(quirks));
  } catch {}
}

function save() {
  try {
    storage?.setItem(key, JSON.stringify(Object.fromEntries([...known].map(([signature, quirks]) => [signature, [...quirks]]))));
  } catch {}
}

export function quirksFor(signature) {
  return new Set(known.get(signature));
}

export function learnQuirk(quirk, signature) {
  if (!known.has(signature)) known.set(signature, new Set());
  if (known.get(signature).has(quirk)) return;
  known.get(signature).add(quirk);
  save();
}

/** Forgets what a model needed, once it no longer fits: a service can change what it accepts. */
export function forgetQuirks(signature) {
  if (known.delete(signature)) save();
}
