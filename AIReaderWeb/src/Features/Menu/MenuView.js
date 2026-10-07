// The reader's menu: the way to everything about the book that is not the
// page itself.
import { pageContext } from "../../Domain/AI/ChatPrompt.js";
import { ChatView } from "../Chat/ChatView.js";
import { Screen } from "../Common/Screen.js";
import { button, h } from "../Common/Ui.js";
import { ContentsView } from "../Contents/ContentsView.js";
import { GlossaryView } from "../Glossary/GlossaryView.js";
import { SearchView } from "../Search/SearchView.js";
import { WordsView } from "../Words/WordsView.js";
import { XRayView } from "../XRay/XRayView.js";
import { DisplayView } from "./DisplayView.js";

export class MenuView extends Screen {
  constructor(env, navigator, reader) {
    super(navigator, "Menu");
    const { feature } = reader;
    const open = (screen) => navigator.push(screen);
    const page = {
      context: pageContext(feature.pageText),
      hint: "Ask about this page — a sentence you can’t parse, a word’s role, what is going on.",
      scope: feature.scope,
    };
    this.setBody(h("nav", { class: "menu" },
      button("Contents", () => open(new ContentsView(navigator, reader))),
      button("Lookups", () => open(new WordsView(env, navigator, feature.book))),
      button("Search", () => open(new SearchView(navigator, reader.link, feature))),
      button("X-ray", () => open(new XRayView(env, navigator, "", reader.link))),
      button("Ask about this page", () => open(new ChatView(env, navigator, page))),
      button("Display", () => open(new DisplayView(env, navigator, feature))),
      feature.document ? button("Offline glossary", () => open(new GlossaryView(env, navigator, feature.book,
        feature.document.chapters.map((chapter) => chapter.text), feature.language))) : null,
      button("Close book", () => navigator.pop(reader))));
  }
}
