// Questions that need an answer before going on, in the platform's own
// modal dialog. The confirming button comes first, so Enter confirms; it
// is shown last.
import { h } from "./Ui.js";

function show(content, actions) {
  return new Promise((resolve) => {
    const dialog = h("dialog", {}, h("form", { method: "dialog" }, content, h("menu", {}, actions)));
    dialog.addEventListener("close", () => {
      dialog.remove();
      resolve(dialog.returnValue);
    });
    document.body.append(dialog);
    dialog.showModal();
  });
}

/** True when the reader confirmed. */
export async function confirmAction({ title, message = "", confirm, cancel = "Cancel" }) {
  const answer = await show(
    [h("h2", {}, title), message ? h("p", {}, message) : null],
    // A stray Enter keeps things as they are.
    [h("button", { value: "confirm" }, confirm), h("button", { value: "cancel", autofocus: true }, cancel)],
  );
  return answer === "confirm";
}

/** Which of the `choices` (`{ value, label }`) the reader took, or null when cancelled. */
export async function chooseAction({ title, message = "", choices, cancel = "Cancel" }) {
  const answer = await show(
    [h("h2", {}, title), message ? h("p", {}, message) : null],
    [...choices.map(({ value, label }) => h("button", { value }, label)), h("button", { value: "", autofocus: true }, cancel)],
  );
  return choices.some(({ value }) => value === answer) ? answer : null;
}

/** The text entered, trimmed, or null when cancelled. */
export async function askText({ title, label, value = "", confirm = "OK" }) {
  const input = h("input", { type: "text", value, required: true, autocomplete: "off" });
  const answer = await show(
    [h("h2", {}, title), h("label", { class: "field" }, h("span", {}, label), input)],
    [h("button", { value: "confirm" }, confirm), h("button", { value: "cancel", formnovalidate: true }, "Cancel")],
  );
  return answer === "confirm" && input.value.trim() ? input.value.trim() : null;
}
