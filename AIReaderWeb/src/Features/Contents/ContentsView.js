// The book's table of contents, the entry the page falls under marked.
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";

export class ContentsView extends Screen {
  constructor(navigator, reader) {
    super(navigator, "Contents");
    const { feature } = reader;
    const contents = feature.document?.contents ?? [];
    const current = feature.contentsEntry;
    this.setBody(contents.length
      ? h("ol", { class: "contents" }, contents.map((entry, index) => h("li", { style: `--depth: ${entry.depth}` },
        button(entry.title, () => {
          navigator.popTo(reader);
          feature.goTo(entry.chapter, entry.offset);
        }, { "aria-current": index === current ? "true" : null }))))
      : note("This book has no table of contents."));
  }

  shown() {
    this.body.querySelector("[aria-current]")?.scrollIntoView({ block: "center" });
  }
}
