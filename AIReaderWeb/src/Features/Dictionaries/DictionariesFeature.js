// The dictionaries the app searches: the bundled one, unless the reader
// removed it, plus any they add — packs as they are, and word lists, Lingvo
// DSL, StarDict and XDXF converted on the way in.
import { Feature } from "../Common/Feature.js";

export class DictionariesFeature extends Feature {
  packs = [];
  /** True once the reader has removed the bundled pack, which the app still ships. */
  canRestoreBundled = false;
  isAdding = false;
  message = "";
  #env;

  constructor(env) {
    super();
    this.#env = env;
  }

  async reload() {
    this.packs = await this.#env.packs.all();
    this.canRestoreBundled = !this.packs.some((pack) => pack.kind === "bundled");
    this.changed();
  }

  async toggle(pack) {
    await this.#env.packs.setEnabled(pack.id, !pack.isEnabled);
    await this.reload();
  }

  async remove(pack) {
    await this.#env.packs.remove(pack);
    await this.reload();
  }

  async restoreBundled() {
    // Gone at once, so a second click cannot add the pack twice.
    this.canRestoreBundled = false;
    this.changed();
    await this.#env.packs.restoreBundled();
    await this.reload();
  }

  async add(files) {
    Object.assign(this, { isAdding: true, message: "" });
    this.changed();
    const { added, errors } = await this.#env.packs.addFiles(files);
    this.isAdding = false;
    if (added === 0 && !errors.length) this.message = "Already in the list.";
    else this.message = [added === 1 ? "Added 1 dictionary." : `Added ${added} dictionaries.`, ...errors].join("\n");
    await this.reload();
  }
}
