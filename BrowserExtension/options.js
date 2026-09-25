import { ext, loadSettings, saveSettings } from "./lib/storage.js";
import { checkFolder } from "./lib/webdav.js";

const element = (id) => document.getElementById(id);

function show(message, isError = false) {
  element("status").textContent = message;
  element("status").className = isError ? "error" : "";
}

element("form").addEventListener("submit", async (event) => {
  event.preventDefault();
  // Asked for first, while the click still counts as the user's: browsers
  // grant permissions only in answer to one.
  const granted = await ext.permissions.request({ origins: ["<all_urls>"] });
  const settings = {
    url: element("url").value.trim(),
    username: element("username").value,
    password: element("password").value,
  };
  await saveSettings(settings);
  if (!granted) {
    show("Saved, but without access to sites nothing can be sent. Save again to allow it.", true);
    return;
  }
  show("Saved. Checking the folder…");
  try {
    const exists = await checkFolder(settings);
    show(exists ? "Saved. The folder is there." : "Saved. The folder is not there yet; it is made when the first book is saved.");
  } catch (error) {
    show(`Saved, but: ${error.message}`, true);
  }
});

const settings = await loadSettings();
element("url").value = settings.url;
element("username").value = settings.username;
element("password").value = settings.password;
