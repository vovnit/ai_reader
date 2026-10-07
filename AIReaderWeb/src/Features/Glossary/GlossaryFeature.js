// A book's offline glossary: count the book's words, then ask the model
// about those the glossary lacks, a batch at a time, filing each answer as
// it arrives — so a stopped or failed run keeps what it paid for, and the
// next asks only about the rest. Lookups use the glossary while it fills.
import { glossaryBatch, glossaryTokensPerWord } from "../../Domain/AI/GlossaryPrompt.js";
import { bookWords } from "../../Domain/Reading/BookWords.js";
import { glossaryName } from "../../Services/DictionaryPacks.js";
import { defineWords } from "../../Services/GlossaryWriter.js";
import { Feature } from "../Common/Feature.js";

/** Requests in flight at once. */
const parallel = 4;

export class GlossaryFeature extends Feature {
  isCounted = false;
  total = 0;
  defined = 0;
  isRunning = false;
  error = "";
  #env;
  #book;
  #chapters;
  #language;
  #missing = [];
  #stopping = false;

  /** `chapters` are the book's texts, `language` its language. */
  constructor(env, book, chapters, language) {
    super();
    this.#env = env;
    this.#book = book;
    this.#chapters = chapters;
    this.#language = language;
    this.name = glossaryName(book);
    this.model = env.settings.ai().model;
  }

  /** What defining the missing words should cost, roughly. */
  get estimatedTokens() {
    return this.#missing.length * glossaryTokensPerWord;
  }

  async count() {
    const words = bookWords(this.#chapters, this.#language);
    const pack = await this.#env.packs.glossary(this.#book);
    const defined = pack ? await this.#env.packs.glossaryForms(pack) : new Set();
    this.#missing = words.filter((word) => !defined.has(word.form));
    this.total = words.length;
    this.defined = words.length - this.#missing.length;
    this.isCounted = true;
    this.changed();
  }

  async start() {
    if (this.isRunning || !this.#missing.length) return;
    this.isRunning = true;
    this.#stopping = false;
    this.error = "";
    this.changed();

    const packs = this.#env.packs;
    const pack = (await packs.glossary(this.#book)) ?? (await packs.createGlossary(this.#book));
    const settings = this.#env.settings.ai();
    const batches = [];
    for (let start = 0; start < this.#missing.length; start += glossaryBatch) batches.push(this.#missing.slice(start, start + glossaryBatch));

    const work = async () => {
      while (!this.#stopping && batches.length) {
        const batch = batches.shift();
        try {
          const definitions = await defineWords(settings, batch);
          await packs.addToGlossary(pack, definitions);
          this.#missing = this.#missing.filter((word) => !definitions.has(word.form));
          this.defined += definitions.size;
        } catch (error) {
          // The rest would most likely fail the same way.
          this.error = error.message;
          this.#stopping = true;
        }
        this.changed();
      }
    };
    await Promise.all(Array.from({ length: parallel }, work));
    this.isRunning = false;
    this.changed();
  }

  /** Lets the requests in flight finish, and starts no more. */
  stop() {
    this.#stopping = true;
  }
}
