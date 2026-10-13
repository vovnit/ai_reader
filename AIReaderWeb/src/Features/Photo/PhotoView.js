// The word a finger points at on a photographed page, explained as a
// tapped word is, with the next photo a tap away; with no finger, the photo
// with the text read over its print, to tap a word in. Opened from the
// shelf, the camera with it, as the iOS app opens it.
import { canSpeak, speak } from "../../Services/Speech.js";
import { fileInput } from "../Common/Files.js";
import { Screen } from "../Common/Screen.js";
import { h, note } from "../Common/Ui.js";
import { lookupBody, LookupView } from "../Lookup/LookupView.js";
import { PhotoFeature } from "./PhotoFeature.js";

export class PhotoView extends Screen {
  #shown = { photo: null, url: "" };

  constructor(env, navigator) {
    super(navigator, "Photo");
    this.env = env;
    this.feature = new PhotoFeature(env);
    this.pronounce = canSpeak() ? this.addAction("Pronounce", () => this.#pronounce(), { title: "Pronounce the word", hidden: true }) : null;
    // A phone's camera app keeps its own flash setting; a page cannot set it.
    this.camera = this.#photoInput("environment");
    this.gallery = this.#photoInput();
    this.addAction("Choose photo", () => this.gallery.click());
    this.next = this.addAction("Take photo", () => this.takePhoto());
    this.watch(this.feature, () => this.#render());
    this.onClose(() => {
      this.feature.close();
      URL.revokeObjectURL(this.#shown.url);
    });
  }

  /** Opens the camera, a picture file's picker elsewhere. Only from a tap: browsers open it for nothing else. */
  takePhoto() {
    this.camera.click();
  }

  /** An input for a photo, the camera's with `capture`. It belongs to the screen, so whatever is picked has a screen to arrive at, however long the picker stays open. */
  #photoInput(capture = "") {
    const input = fileInput({ accept: "image/*", capture });
    input.addEventListener("change", () => {
      const [photo] = input.files;
      // Cleared, so the same picture can be picked again.
      input.value = "";
      if (photo) this.feature.read(photo);
    });
    this.element.append(input);
    return input;
  }

  #pronounce() {
    const context = this.feature.lookup?.context;
    if (context) speak(context.word, context.language);
  }

  #render() {
    const { hasPhoto, status, error, lookup, passage } = this.feature;
    this.setTitle(lookup?.context.word ?? "Photo");
    this.next.textContent = hasPhoto ? "Next photo" : "Take photo";
    if (this.pronounce) this.pronounce.hidden = !lookup;
    if (!hasPhoto) {
      this.setBody(note("Photograph the page with a finger just below the word you mean."));
    } else if (status) {
      this.setBody(note(status === "preparing" ? "Getting the text reader ready…" : "Reading the photo…"));
    } else if (error) {
      this.setBody(h("h2", {}, "Nothing to read"), h("p", { class: "error message" }, error));
    } else if (lookup) {
      this.setBody(...lookupBody(this.env, this.navigator, lookup));
    } else if (passage) {
      const words = this.#photoWords(passage);
      this.setBody(note("No finger pointed at a word. Tap one to explain it."), words);
      fit(words);
    }
  }

  /** The photo with the text read off it laid over its print, each word where it was read, to tap one. */
  #photoWords({ words, photoSize }) {
    const page = svg("svg", { viewBox: `0 0 ${photoSize.width} ${photoSize.height}` });
    for (const { text, offset, center, size, angle } of words) {
      const word = svg("g", { transform: `rotate(${(angle * 180) / Math.PI} ${center.x} ${center.y})` });
      // Tesseract's boxes hug the letters, leaving out descenders and the
      // soft edges of strokes, which the patch reaches past to hide.
      const patch = { width: size.width + 0.2 * size.height, height: 1.5 * size.height };
      word.append(
        svg("rect", { x: center.x - patch.width / 2, y: center.y - patch.height / 2, width: patch.width, height: patch.height }),
        svg("text", {
          x: center.x, y: center.y, "font-size": size.height * 0.8, "data-length": size.width,
          "text-anchor": "middle", "dominant-baseline": "central",
        }, text),
      );
      word.addEventListener("click", () => this.#explain(offset));
      page.append(word);
    }
    return h("div", { class: "photo-words" }, h("img", { src: this.#urlOf(this.feature.photo), alt: "" }), page);
  }

  #explain(offset) {
    const word = this.feature.wordAt(offset);
    if (!word) return;
    this.navigator.popTo(this);
    this.navigator.push(new LookupView(this.env, this.navigator, word));
  }

  /** An address for the photo, made once for each photo shown. */
  #urlOf(photo) {
    if (this.#shown.photo !== photo) {
      URL.revokeObjectURL(this.#shown.url);
      this.#shown = { photo, url: URL.createObjectURL(photo) };
    }
    return this.#shown.url;
  }
}

/** Shrinks each word drawn longer than its print to the print's length, as the iOS app does. Only words on the page can be measured. */
function fit(words) {
  const texts = [...words.querySelectorAll("text")];
  // All measured before any changes, so the page is laid out once.
  const lengths = texts.map((text) => text.getComputedTextLength());
  texts.forEach((text, index) => {
    const room = Number(text.dataset.length);
    if (lengths[index] > room) text.setAttribute("font-size", (Number(text.getAttribute("font-size")) * room) / lengths[index]);
  });
}

/** An SVG element, which `h` cannot make. */
function svg(tag, attributes, ...children) {
  const element = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, String(value));
  element.append(...children);
  return element;
}
