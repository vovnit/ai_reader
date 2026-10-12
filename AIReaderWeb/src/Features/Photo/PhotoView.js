// The word a finger points at on a photographed page, explained as a
// tapped word is, with the next photo a tap away; with no finger, the
// page's text to tap a word in. Opened from the shelf, the camera with it,
// as the iOS app opens it.
import { canSpeak, speak } from "../../Services/Speech.js";
import { fileInput } from "../Common/Files.js";
import { Screen } from "../Common/Screen.js";
import { h, note } from "../Common/Ui.js";
import { lookupBody, LookupView } from "../Lookup/LookupView.js";
import { PhotoFeature } from "./PhotoFeature.js";

export class PhotoView extends Screen {
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
    this.onClose(() => this.feature.close());
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
      this.setBody(note("No finger pointed at a word. Tap one to explain it."), this.#passage(passage.text));
    }
  }

  #passage(text) {
    const paragraph = h("p", { class: "message" }, text);
    paragraph.addEventListener("click", (event) => {
      const offset = offsetAt(paragraph.firstChild, event.clientX, event.clientY);
      const word = offset === null ? null : this.feature.wordAt(offset);
      if (!word) return;
      this.navigator.popTo(this);
      this.navigator.push(new LookupView(this.env, this.navigator, word));
    });
    return paragraph;
  }
}

/** The offset in the text node `node` of the character under a point, or null when the point is not on one of its characters. */
function offsetAt(node, x, y) {
  const position = document.caretPositionFromPoint?.(x, y);
  const range = position ? null : document.caretRangeFromPoint?.(x, y);
  if ((position?.offsetNode ?? range?.startContainer) !== node) return null;
  const offset = position?.offset ?? range?.startOffset;
  // The caret sits between characters; only a point on one of them counts.
  const probe = document.createRange();
  for (const index of [offset, offset - 1]) {
    if (index < 0 || index >= node.length) continue;
    probe.setStart(node, index);
    probe.setEnd(node, index + 1);
    for (const rect of probe.getClientRects()) {
      if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom) return index;
    }
  }
  return null;
}
