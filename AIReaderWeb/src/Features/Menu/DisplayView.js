// How the book is set: size, face, line spacing and margins, each a step at
// a time, shown on the page as it changes.
import { fontFamily, fontNames, ranges } from "../../Domain/Reading/ReadingStyle.js";
import { Screen } from "../Common/Screen.js";
import { button, field, h } from "../Common/Ui.js";
import { DisplayFeature } from "./DisplayFeature.js";

export class DisplayView extends Screen {
  constructor(env, navigator, reader) {
    super(navigator, "Display");
    this.feature = new DisplayFeature(env, reader);
    this.watch(this.feature, () => this.#render());
  }

  #stepper(label, fieldName, format) {
    const { style } = this.feature;
    const { min, max } = ranges[fieldName];
    return h("div", { class: "stepper" },
      h("span", {}, label),
      button("−", () => this.feature.adjust(fieldName, -1), { "aria-label": `Less ${label.toLowerCase()}`, disabled: style[fieldName] <= min }),
      h("output", {}, format(style[fieldName])),
      button("+", () => this.feature.adjust(fieldName, 1), { "aria-label": `More ${label.toLowerCase()}`, disabled: style[fieldName] >= max }));
  }

  #render() {
    const { style } = this.feature;
    const face = h("select", { onchange: (event) => this.feature.setFont(event.target.value) },
      fontNames.map((name) => h("option", { value: name, selected: (style.fontName || "Serif") === name }, name)));
    this.setBody(
      this.#stepper("Size", "scale", (value) => `${Math.round(value * 100)}%`),
      field("Face", face),
      this.#stepper("Line spacing", "lineSpacing", (value) => `${value}`),
      this.#stepper("Margins", "margin", (value) => `${value}`),
      h("p", { class: "sample", style: `font-family: ${fontFamily(style)}` }, "The quick brown fox jumps over the lazy dog."));
  }
}
