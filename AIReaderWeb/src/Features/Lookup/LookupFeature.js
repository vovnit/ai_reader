// One word lookup: read the cache, otherwise ask the explainer and store
// the answer. The dictionary's own entry for the lemma comes with it, so
// the reader can see what the answer was drawn from. When the model cannot
// be asked, the dictionaries answer alone.
import { toolsFor } from "../../App/Env.js";
import { explainWord } from "../../Services/WordExplainer.js";
import { Feature } from "../Common/Feature.js";

export class LookupFeature extends Feature {
  explanation = null;
  /** The articles under the lemma; none when the dictionary has no such headword, as after a guess. Without an explanation, what the dictionaries have for the word itself. */
  entry = [];
  error = "";
  #env;

  /** `context` is `{ word, sentence, language, bookId }`; `scope` what the model may search, if a book is open. */
  constructor(env, context, scope = {}) {
    super();
    this.#env = env;
    this.context = context;
    this.scope = scope;
  }

  async start() {
    const tools = await toolsFor(this.#env, this.scope);
    try {
      const cached = await this.#env.lookups.cached(this.context);
      this.explanation = cached ?? (await explainWord(this.context.word, this.context.sentence, this.#env.settings.ai(), tools));
      if (!cached) await this.#env.lookups.save(this.context, this.explanation);
      this.entry = await this.#env.dictionary.articlesFor(this.explanation.lemma, tools.packs);
    } catch (error) {
      this.error = error.message;
      // Offline, or the model unreachable: the dictionaries, a book's own
      // glossary among them, are on the device.
      if (!this.explanation) {
        this.entry = await this.#env.dictionary.lookup(this.context.word, tools.packs).then((found) => found.articles, () => []);
      }
    }
    this.changed();
  }
}
