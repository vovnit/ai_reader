// A conversation about something in front of the reader: the page, a word
// just explained, what the book says about a name. The context is sent
// once, with the first question, so follow-ups cost only the thread so far.
// The model may open the dictionary and search the book while answering.
import { toolsFor } from "../../App/Env.js";
import { chatMessages, chatTools } from "../../Domain/AI/ChatPrompt.js";
import { converse } from "../../Services/ToolRunner.js";
import { Feature } from "../Common/Feature.js";

export class ChatFeature extends Feature {
  /** `{ isReader, text }`, in order. */
  turns = [];
  isAnswering = false;
  error = "";
  #env;

  constructor(env, context, scope = {}) {
    super();
    this.#env = env;
    this.context = context;
    this.scope = scope;
  }

  /** Sends a question; an empty one, or one asked while answering, is ignored. */
  async send(question) {
    const text = question.trim();
    if (!text || this.isAnswering) return;
    this.turns = [...this.turns, { isReader: true, text }];
    this.isAnswering = true;
    this.error = "";
    this.changed();
    const settings = this.#env.settings.ai();
    try {
      const messages = chatMessages(this.context, this.turns, settings.language);
      const reply = await converse(settings, messages, chatTools, false, await toolsFor(this.#env, this.scope));
      this.turns = [...this.turns, { isReader: false, text: reply.content ?? "" }];
    } catch (error) {
      this.error = error.message;
    }
    this.isAnswering = false;
    this.changed();
  }
}
