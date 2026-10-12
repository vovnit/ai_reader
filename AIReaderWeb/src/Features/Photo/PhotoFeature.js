// A word on a printed page: the reader photographs the page with a finger
// under the word, the photo's text is read on the device, and the word the
// finger points at is explained in its sentence — the lookup a book gives,
// and from there the next photo. There is no book, so the lookup is filed
// under none. The iOS app's PhotoFeature.
import { detectLanguage, languageCode } from "../../Domain/Books/LanguageDetector.js";
import { photoPassage } from "../../Domain/Photo/PhotoPassage.js";
import { selectionAt } from "../../Domain/Reading/WordContext.js";
import { canRead, PhotoReader } from "../../Services/PhotoReader.js";
import { Feature } from "../Common/Feature.js";
import { LookupFeature } from "../Lookup/LookupFeature.js";

export class PhotoFeature extends Feature {
  /** "preparing" while the language's model is fetched and started, "reading" while the photo is read, else "". */
  status = "";
  error = "";
  /** The photo's text, and where in it the pointed word starts; null before a photo is read. */
  passage = null;
  /** What the pointed word means here; null when no finger pointed at a word. */
  lookup = null;
  #env;
  #reader;
  #photos = 0;
  #queue = Promise.resolve();
  #stopLookup = () => {};

  /** `reader` reads photos, as `PhotoReader` does. */
  constructor(env, reader = new PhotoReader()) {
    super();
    this.#env = env;
    this.#reader = reader;
  }

  /** Reads the photo `file` and explains the word pointed at. A photo taken while another is read is read after it; one taken after that wins. */
  read(file) {
    const photo = ++this.#photos;
    this.status = "reading";
    this.changed();
    this.#queue = this.#queue.then(() => (photo === this.#photos ? this.#read(file) : undefined));
    return this.#queue;
  }

  async #read(file) {
    this.#stopLookup();
    this.lookup = null;
    this.passage = null;
    this.error = "";
    this.status = "reading";
    this.changed();
    const onStatus = (status) => {
      this.status = status;
      this.changed();
    };
    try {
      let language = this.#env.settings.photoLanguage() || (await this.#shelfLanguage());
      let passage = photoPassage(await this.#reader.read(file, language, onStatus));
      // A page in another language reads as garbled words of this one, which
      // still show which language it is: read again in that one.
      const found = detectLanguage([{ text: passage.text }]);
      if (found && found !== language && canRead(found)) {
        language = found;
        // Offline that language's model may not be here yet; the first
        // reading, rougher, still has the word.
        passage = await this.#reader.read(file, language, onStatus).then(photoPassage, () => passage);
      }
      this.#env.settings.savePhotoLanguage(language);
      if (!passage.text) {
        this.error = "No text was found in the photo.";
      } else {
        this.passage = passage;
        this.#explain(passage, language);
      }
    } catch (error) {
      this.error = error.message;
    }
    this.status = "";
    this.changed();
  }

  /** The first photo is read in the language of the book read last, as the reader is likely to photograph another like it. */
  async #shelfLanguage() {
    const books = await this.#env.library.all();
    const last = books.reduce((latest, book) => ((book.updatedAt || "") > (latest?.updatedAt || "") ? book : latest), books[0]);
    const language = languageCode(last?.language ?? "");
    return canRead(language) ? language : "en";
  }

  #explain(passage, language) {
    if (passage.pointedOffset === null) return;
    const selection = selectionAt(passage.text, passage.pointedOffset, language);
    if (!selection) return;
    this.lookup = new LookupFeature(this.#env, { word: selection.word, sentence: selection.sentence, language });
    this.#stopLookup = this.lookup.subscribe(() => this.changed());
    this.lookup.start();
  }

  /** Stops reading photos, and frees the text reader. */
  close() {
    this.#stopLookup();
    this.#reader.close();
  }
}
