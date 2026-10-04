// Type size, face, line spacing and margins. A change is saved and applied
// at once: the reader lays the chapter out again on the same page.
import { stepped } from "../../Domain/Reading/ReadingStyle.js";
import { Feature } from "../Common/Feature.js";

export class DisplayFeature extends Feature {
  #env;
  #reader;

  constructor(env, reader) {
    super();
    this.#env = env;
    this.#reader = reader;
    this.style = env.settings.style();
  }

  adjust(field, direction) {
    this.#apply(stepped(this.style, field, direction));
  }

  setFont(name) {
    this.#apply({ ...this.style, fontName: name === "Serif" ? "" : name });
  }

  #apply(style) {
    this.style = style;
    this.#env.settings.saveStyle(style);
    this.#reader?.setStyle(style);
    this.changed();
  }
}
