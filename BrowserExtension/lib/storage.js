// What the extension keeps: the WebDAV settings, and the book being
// composed, page by page, until it is saved.

export const ext = globalThis.browser ?? globalThis.chrome;

export async function loadSettings() {
  const { settings } = await ext.storage.local.get("settings");
  return { url: "", username: "", password: "", ...settings };
}

export async function saveSettings(settings) {
  await ext.storage.local.set({ settings });
}

// Storage holds JSON, so image bytes travel as base64.
function toBase64(bytes) {
  let binary = "";
  for (let i = 0; i < bytes.length; i += 0x8000) binary += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(binary);
}

function fromBase64(text) {
  return Uint8Array.from(atob(text), (character) => character.charCodeAt(0));
}

/** The book being composed: `{ title, pages }`, pages as `capturePage` makes them. */
export async function loadDraft() {
  const { draft } = await ext.storage.local.get("draft");
  if (!draft) return { title: "", pages: [] };
  return {
    title: draft.title,
    pages: draft.pages.map((page) => ({
      ...page,
      images: page.images.map((image) => ({ ...image, bytes: fromBase64(image.bytes) })),
    })),
  };
}

export async function saveDraft(draft) {
  await ext.storage.local.set({
    draft: {
      title: draft.title,
      pages: draft.pages.map((page) => ({
        ...page,
        images: page.images.map((image) => ({ ...image, bytes: toBase64(image.bytes) })),
      })),
    },
  });
}

export async function clearDraft() {
  await ext.storage.local.remove("draft");
}
