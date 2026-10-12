// The word a finger points at on a photographed page, explained as a
// tapped word is, with the next photo a tap away. Opened from the shelf
// once the first photo is taken.
import { canSpeak, speak } from "../../Services/Speech.js";
import { pickFiles } from "../Common/Files.js";
import { Screen } from "../Common/Screen.js";
import { h, note } from "../Common/Ui.js";
import { lookupBody } from "../Lookup/LookupView.js";
import { PhotoFeature } from "./PhotoFeature.js";

/** A photo from a phone's camera, or a picture file elsewhere; null when the reader cancelled. */
export async function takePhoto() {
  const [photo] = await pickFiles({ accept: "image/*", multiple: false, capture: "environment" });
  return photo ?? null;
}

export class PhotoView extends Screen {
  constructor(env, navigator, photo) {
    super(navigator, "Photo");
    this.env = env;
    this.feature = new PhotoFeature(env);
    this.pronounce = canSpeak() ? this.addAction("Pronounce", () => this.#pronounce(), { title: "Pronounce the word", hidden: true }) : null;
    this.addAction("Next photo", () => this.#next());
    this.watch(this.feature, () => this.#render());
    this.onClose(() => this.feature.close());
    this.feature.read(photo);
  }

  async #next() {
    const photo = await takePhoto();
    if (photo) this.feature.read(photo);
  }

  #pronounce() {
    const context = this.feature.lookup?.context;
    if (context) speak(context.word, context.language);
  }

  #render() {
    const { status, error, lookup } = this.feature;
    this.setTitle(lookup?.context.word ?? "Photo");
    if (this.pronounce) this.pronounce.hidden = !lookup;
    if (status) {
      this.setBody(note(status === "preparing" ? "Getting the text reader ready…" : "Reading the photo…"));
    } else if (error) {
      this.setBody(h("h2", {}, "Nothing to read"), h("p", { class: "error message" }, error));
    } else if (lookup) {
      this.setBody(...lookupBody(this.env, this.navigator, lookup));
    } else {
      this.setBody(h("h2", {}, "No word pointed at"), note("Hold a finger just below the word you mean, and take the photo again."));
    }
  }
}
