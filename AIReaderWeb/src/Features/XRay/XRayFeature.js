// What the book itself says a name or a word is: the passages where it has
// appeared so far are gathered and the model is asked to read them.
// Nothing past the page on screen is shown or sent, so nothing is given away.
import { toolsFor } from "../../App/Env.js";
import { passageLimit } from "../../Domain/AI/SearchSummary.js";
import { searchTool } from "../../Domain/AI/Tools.js";
import { xrayMessages } from "../../Domain/AI/XRayPrompt.js";
import { converse } from "../../Services/ToolRunner.js";
import { Feature } from "../Common/Feature.js";

export class XRayFeature extends Feature {
  term = "";
  /** The passages the answer was drawn from, in reading order. */
  passages = [];
  answer = "";
  error = "";
  isWorking = false;
  #env;

  constructor(env, scope) {
    super();
    this.#env = env;
    this.scope = scope;
  }

  async ask(term) {
    const wanted = term.trim();
    if (!wanted || this.isWorking) return;
    Object.assign(this, { term: wanted, passages: [], answer: "", error: "", isWorking: true });
    this.changed();
    const { corpus, upTo } = this.scope;
    if (!corpus) {
      Object.assign(this, { error: "No book is open to read from.", isWorking: false });
      return this.changed();
    }
    this.passages = await corpus.search(wanted, passageLimit, upTo);
    try {
      const settings = this.#env.settings.ai();
      const tools = { ...(await toolsFor(this.#env, this.scope)), packs: [] };
      const messages = xrayMessages(wanted, this.passages, corpus.severalBooks, settings.language);
      this.answer = (await converse(settings, messages, [searchTool], false, tools)).content ?? "";
    } catch (error) {
      this.error = error.message;
    }
    this.error = [this.error, ...corpus.errors()].filter(Boolean).join("\n");
    this.isWorking = false;
    this.changed();
  }
}
