// A word or a phrase anywhere in the book, or in every book of its group:
// the sentences it occurs in, each a click from its place.
import { Screen } from "../Common/Screen.js";
import { h, marked, note } from "../Common/Ui.js";
import { SearchFeature } from "./SearchFeature.js";

export class SearchView extends Screen {
  constructor(navigator, link, reader) {
    super(navigator, "Search");
    this.link = link;
    this.feature = new SearchFeature(reader.corpus);
    const books = reader.corpus?.books.length ?? 1;
    this.covers = books > 1 ? `Searches the ${books} books of the group.` : "Searches this book.";
    this.input = h("input", { type: "search", placeholder: "A word or a phrase", enterkeyhint: "search" });
    this.form = h("form", { class: "ask", onsubmit: (event) => {
      event.preventDefault();
      this.feature.search(this.input.value);
    } }, this.input, h("button", {}, "Search"));
    this.list = h("ul", { class: "hits" });
    this.status = h("div");
    this.setBody(this.form, this.status, this.list);
    this.watch(this.feature, () => this.#render());
  }

  shown() {
    this.input.focus();
  }

  #render() {
    const { query, hits, isSearching, error, severalBooks } = this.feature;
    this.status.replaceChildren(...[
      note(isSearching ? "Searching…" : query && !hits.length ? `Nothing found for “${query}”.` : query ? `${hits.length}${hits.length === 100 ? "+" : ""} found.` : this.covers),
      error ? h("p", { class: "error message" }, error) : null,
    ].filter(Boolean));
    this.list.replaceChildren(...hits.map((hit) => h("li", {}, h("button", { type: "button", onclick: () => this.link.goTo(hit) },
      h("small", {}, `${severalBooks ? `${hit.bookTitle} · ` : ""}Chapter ${hit.chapter + 1}`),
      h("span", {}, marked(hit.excerpt, hit.matchStart, hit.matchEnd))))));
  }
}
