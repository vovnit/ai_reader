// Building plain HTML elements without markup strings, so nothing a book
// or a model says is ever read as markup.

const properties = new Set(["value", "checked", "disabled", "selected", "hidden", "open"]);

/** `h("button", { class, onclick, ... }, ...children)`; null and false children are left out. */
export function h(tag, props = {}, ...children) {
  const element = document.createElement(tag);
  for (const [key, value] of Object.entries(props ?? {})) {
    if (value === undefined || value === null || value === false) continue;
    if (key.startsWith("on")) element.addEventListener(key.slice(2), value);
    else if (key === "class") element.className = value;
    else if (properties.has(key)) element[key] = value;
    else element.setAttribute(key, value === true ? "" : String(value));
  }
  element.append(...children.flat(Infinity).filter((child) => child !== null && child !== undefined && child !== false));
  return element;
}

export function button(label, action, props = {}) {
  return h("button", { type: "button", onclick: action, ...props }, label);
}

/** A labelled control, the label above it. */
export function field(label, control, note = "") {
  return h("label", { class: "field" }, h("span", {}, label), control, note ? h("small", {}, note) : null);
}

/** A line of muted text, or nothing when there is nothing to say. */
export function note(text, props = {}) {
  return text ? h("p", { class: "note", ...props }, text) : null;
}

/** Text with the stretch `[start, end)` marked, as a search hit shows its match. */
export function marked(text, start, end) {
  return [text.slice(0, start), h("mark", {}, text.slice(start, end)), text.slice(end)];
}
