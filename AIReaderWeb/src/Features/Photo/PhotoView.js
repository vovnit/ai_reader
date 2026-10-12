// The word a finger points at on a photographed page, explained as a
// tapped word is, with the next photo a tap away. Opened from the shelf,
// the camera with it, as the iOS app opens it.
import { canSpeak, speak } from "../../Services/Speech.js";
import { fileInput } from "../Common/Files.js";
import { Screen } from "../Common/Screen.js";
import { h, note } from "../Common/Ui.js";
import { lookupBody } from "../Lookup/LookupView.js";
import { PhotoFeature } from "./PhotoFeature.js";

export class PhotoView extends Screen {
  constructor(env, navigator) {
    super(navigator, "Photo");
    this.env = env;
    this.feature = new PhotoFeature(env);
    this.pronounce = canSpeak() ? this.addAction("Pronounce", () => this.#pronounce(), { title: "Pronounce the word", hidden: true }) : null;
    this.next = this.addAction("Take photo", () => this.takePhoto());
    // The camera's input belongs to the screen, so whatever the camera
    // takes has a screen to arrive at, however long it stays open.
    this.camera = fileInput({ accept: "image/*", capture: "environment" });
    this.camera.addEventListener("change", () => {
      const [photo] = this.camera.files;
      // Cleared, so the same picture can be picked again.
      this.camera.value = "";
      if (photo) this.feature.read(photo);
    });
    this.element.append(this.camera);
    this.watch(this.feature, () => this.#render());
    this.onClose(() => this.feature.close());
  }

  /** Opens the camera, a picture file's picker elsewhere. Only from a tap: browsers open it for nothing else. */
  takePhoto() {
    this.camera.click();
  }

  #pronounce() {
    const context = this.feature.lookup?.context;
    if (context) speak(context.word, context.language);
  }

  #render() {
    const { hasPhoto, status, error, lookup } = this.feature;
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
    } else {
      this.setBody(h("h2", {}, "No word pointed at"), note("Hold a finger just below the word you mean, and take the photo again."));
    }
  }
}
