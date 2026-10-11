// Reads a photo of a page on the device, as Vision does for the iOS app:
// its text with Tesseract.js, kept in vendor/ and run in a worker of its
// own, and a finger pointing into it, found in its pixels. A language's
// model is fetched the first time a photo needs it, and the browser keeps it.
import { findFinger } from "../Domain/Photo/FingerFinder.js";
import { tesseractLines } from "../Domain/Photo/TesseractPage.js";
import { RgbImage } from "../Support/RgbImage.js";

const vendor = new URL("../../vendor/tesseract/", import.meta.url).href;
/** Tesseract's names for the languages `detectLanguage` tells apart. */
const models = { en: "eng", fr: "fra", de: "deu", es: "spa", it: "ita", pt: "por", ru: "rus", nl: "nld" };
// A phone's photo is 4000 pixels long; Tesseract reads a page's print as
// well at 1600, in a fraction of the time (measured on the finder's cases).
const longSide = 1600;
const lstmOnly = 1;
// Columns, captions and a page's edge are found as Vision finds them;
// Tesseract.js's own default reads the photo as one block of text.
const automaticLayout = "3";

/** Whether a page in `language`, a two-letter code, can be read. */
export const canRead = (language) => Object.hasOwn(models, language);

export class PhotoReader {
  /** `{ model, worker }`, the worker a promise; null until a photo is read. */
  #current = null;

  /**
   * `{ lines, fingers }` of the photo `file`, in the pixels of the copy that
   * was read; its page is in `language`, a two-letter code. `onStatus` hears
   * "preparing" while the language's model is fetched and started, then
   * "reading". Throws why the photo cannot be read.
   */
  async read(file, language, onStatus = () => {}) {
    const page = await upright(file);
    const model = models[language] ?? models.en;
    if (this.#current?.model !== model) {
      this.close();
      onStatus("preparing");
      this.#current = { model, worker: start(model) };
    }
    let worker;
    try {
      worker = await this.#current.worker;
    } catch (error) {
      this.#current = null;
      // A worker that cannot be loaded at all says nothing more.
      const why = error instanceof Error ? error.message : String(error ?? "it could not be loaded");
      throw new Error(`The text reader couldn’t be started — the first photo in a language needs the network: ${why}`);
    }
    onStatus("reading");
    const { data } = await worker.recognize(page.png, {}, { blocks: true, text: false });
    const lines = tesseractLines(data.blocks ?? []);
    const size = { width: page.width, height: page.height };
    const finger = findFinger(RgbImage.fromRgba(page.pixels, page.width, page.height).shrunk(256), lines, size);
    return { lines, fingers: finger ? [finger] : [] };
  }

  /** Stops the worker, freeing the memory its model takes. */
  close() {
    this.#current?.worker.then((worker) => worker.terminate(), () => {});
    this.#current = null;
  }
}

let failedImports = 0;

async function start(model) {
  // A module that failed to load stays failed for as long as the page is
  // open, so after a failure, offline say, it is asked for at another address.
  const address = `../../vendor/tesseract/tesseract.esm.min.js${failedImports ? `?again=${failedImports}` : ""}`;
  const { createWorker } = (await import(address).catch((error) => {
    failedImports++;
    throw error;
  })).default;
  // A worker whose model cannot be fetched reports it here and never
  // finishes starting, so the report ends the wait.
  let failed;
  const failure = new Promise((resolve, reject) => (failed = reject));
  const worker = await Promise.race([
    createWorker(model, lstmOnly, {
      // Loaded from its own address, the worker finds the core's .wasm beside it.
      workerPath: `${vendor}worker.min.js`,
      workerBlobURL: false,
      corePath: `${vendor}tesseract-core-simd-lstm.js`,
      errorHandler: (message) => failed(new Error(String(message))),
    }),
    failure,
  ]);
  await worker.setParameters({ tessedit_pageseg_mode: automaticLayout });
  return worker;
}

/** The photo turned upright and shrunk to at most `longSide`: its RGBA pixels, and the same as a PNG for Tesseract. */
async function upright(file) {
  let bitmap;
  try {
    bitmap = await createImageBitmap(file, { imageOrientation: "from-image" });
  } catch {
    throw new Error("The photo couldn’t be opened.");
  }
  const scale = Math.min(1, longSide / Math.max(bitmap.width, bitmap.height));
  const width = Math.max(1, Math.round(bitmap.width * scale));
  const height = Math.max(1, Math.round(bitmap.height * scale));
  const canvas = new OffscreenCanvas(width, height);
  const context = canvas.getContext("2d");
  context.imageSmoothingQuality = "high";
  context.drawImage(bitmap, 0, 0, width, height);
  bitmap.close();
  const pixels = context.getImageData(0, 0, width, height).data;
  return { width, height, pixels, png: await canvas.convertToBlob({ type: "image/png" }) };
}
