import { addPage, removePage, renameDraft, saveComposed, savePage } from "./lib/books.js";
import { clearDraft, ext, loadDraft, loadSettings } from "./lib/storage.js";

const element = (id) => document.getElementById(id);
let settings;
let draft;
let tab;
let hasAccess = false;

async function extract() {
  const [result] = await ext.scripting.executeScript({ target: { tabId: tab.id }, files: ["extract.js"] });
  if (!result?.result?.xhtml) throw new Error("This page cannot be read.");
  return result.result;
}

function render() {
  const ready = Boolean(settings.url.trim()) && hasAccess;
  element("setup").hidden = ready;
  element("save-page").disabled = !ready;
  element("save-draft").disabled = !ready;

  element("draft").hidden = draft.pages.length === 0;
  element("add-page").textContent = draft.pages.length === 0 ? "Start a book with this page" : "Add to book in progress";
  const title = element("draft-title");
  if (document.activeElement !== title) title.value = draft.title;
  const list = element("draft-pages");
  list.replaceChildren(
    ...draft.pages.map((page, index) => {
      const item = document.createElement("li");
      const name = document.createElement("span");
      name.textContent = page.title;
      const remove = document.createElement("button");
      remove.type = "button";
      remove.textContent = "✕";
      remove.title = "Remove this page";
      remove.addEventListener("click", () => run(async () => {
        draft = await removePage(index);
        return "";
      }));
      item.append(name, remove);
      return item;
    })
  );
}

function show(message, isError = false) {
  element("status").textContent = message;
  element("status").classList.toggle("error", isError);
}

/** Runs one action with the buttons held, and says how it went. */
async function run(work, pending = "") {
  const buttons = [...document.querySelectorAll("button")];
  buttons.forEach((button) => (button.disabled = true));
  show(pending);
  try {
    show(await work());
  } catch (error) {
    show(error.message || String(error), true);
  } finally {
    buttons.forEach((button) => (button.disabled = false));
    render();
  }
}

const saved = (name) => `Saved “${name}”. It arrives on the next sync.`;

element("save-page").addEventListener("click", () =>
  run(async () => saved(await savePage(await extract(), settings)), "Saving…")
);

element("add-page").addEventListener("click", () =>
  run(async () => {
    draft = await addPage(await extract());
    return `Added. The book has ${draft.pages.length} page${draft.pages.length === 1 ? "" : "s"}.`;
  }, "Adding…")
);

element("save-draft").addEventListener("click", () =>
  run(async () => {
    const name = await saveComposed(settings);
    draft = await loadDraft();
    return saved(name);
  }, "Saving…")
);

element("discard-draft").addEventListener("click", () => {
  if (!confirm("Discard the book in progress?")) return;
  run(async () => {
    await clearDraft();
    draft = await loadDraft();
    return "";
  });
});

element("draft-title").addEventListener("change", (event) => {
  draft.title = event.target.value;
  renameDraft(draft.title);
});

for (const id of ["open-settings", "settings-link"]) {
  element(id).addEventListener("click", (event) => {
    event.preventDefault();
    ext.runtime.openOptionsPage();
    window.close();
  });
}

[tab] = await ext.tabs.query({ active: true, currentWindow: true });
settings = await loadSettings();
draft = await loadDraft();
hasAccess = await ext.permissions.contains({ origins: ["<all_urls>"] });
element("page-title").textContent = tab?.title || "This page";
render();
