// The shelf, each group under its name, then the books in the sync folder
// that are not here. A book opens the reader; its buttons put it in a group
// or take it off this device, and its file out of the sync folder.
import { chooseAction, confirmAction } from "../Common/Dialogs.js";
import { pickFiles } from "../Common/Files.js";
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";
import { ReaderView } from "../Reader/ReaderView.js";
import { SettingsView } from "../Settings/SettingsView.js";
import { WordsView } from "../Words/WordsView.js";
import { GroupView } from "./GroupView.js";
import { LibraryFeature, remoteTitle } from "./LibraryFeature.js";

export class LibraryView extends Screen {
  backName = "Library";
  #covers = new Map();

  constructor(env, navigator) {
    super(navigator, "Library", { placement: "main" });
    this.env = env;
    this.feature = new LibraryFeature(env);
    this.addAction("Add…", () => this.#pick());
    this.addAction("Words", () => navigator.push(new WordsView(env, navigator, null)));
    this.addAction("Settings", () => navigator.push(new SettingsView(env, navigator)));
    this.#acceptDrops();
    this.watch(this.feature, () => this.#render());
    this.onClose(() => this.#covers.forEach((url) => URL.revokeObjectURL(url)));
  }

  shown() {
    this.feature.reload();
  }

  /** Books handed in from outside — dropped, or opened with the installed app — go on the shelf, and the first one opens. */
  async receive(files) {
    const [first] = await this.feature.add(files);
    if (first) this.open(this.feature.books.find((book) => book.id === first));
  }

  open(book) {
    if (book) this.navigator.push(new ReaderView(this.env, this.navigator, book, () => this.feature.sync()));
  }

  async #pick() {
    const files = await pickFiles({ accept: ".epub,application/epub+zip,.pdf,application/pdf" });
    if (files.length) await this.feature.add(files);
  }

  #acceptDrops() {
    this.element.addEventListener("dragover", (event) => event.preventDefault());
    this.element.addEventListener("drop", (event) => {
      event.preventDefault();
      const files = [...event.dataTransfer.files].filter((file) => /\.(epub|pdf)$/i.test(file.name));
      if (files.length) this.feature.add(files);
    });
  }

  #cover(book) {
    if (!this.#covers.has(book.id)) {
      this.#covers.set(book.id, null);
      this.env.library.cover(book.id).then((blob) => {
        if (!blob) return;
        this.#covers.set(book.id, URL.createObjectURL(blob));
        this.#render();
      });
    }
    const url = this.#covers.get(book.id);
    return url ? h("img", { src: url, alt: "", loading: "lazy" }) : h("span", { class: "no-cover" });
  }

  #row(book) {
    return h("li", { class: "book" },
      h("button", { type: "button", class: "open", onclick: () => this.open(book) },
        this.#cover(book),
        h("span", {}, h("strong", {}, book.title), book.author ? h("small", {}, book.author) : null)),
      h("span", { class: "row-actions" },
        button("Group…", () => this.navigator.push(new GroupView(this.navigator, this.feature, book))),
        button("Remove", () => this.#remove(book))));
  }

  /** A book in the sync folder only: fetched on request. */
  #cloudRow(name) {
    const downloading = this.feature.downloading.has(name);
    return h("li", { class: "book" },
      h("span", { class: "remote" }, h("strong", {}, remoteTitle(name)), downloading ? h("small", {}, "Downloading…") : null),
      h("span", { class: "row-actions" },
        button("Download", () => this.feature.download(name), { disabled: downloading }),
        button("Delete…", () => this.#removeRemote(name))));
  }

  async #remove(book) {
    if (!this.feature.isInCloud(book)) {
      const ok = await confirmAction({ title: `Remove “${book.title}”?`, message: "It is removed from this device. Words looked up in it are kept.", confirm: "Remove" });
      if (ok) await this.feature.remove(book);
      return;
    }
    const choice = await chooseAction({
      title: `Remove “${book.title}”?`,
      message: "Its copy in the sync folder can stay for your other devices, or go too. Devices that already have it keep theirs. Words looked up in it are kept.",
      choices: [{ value: "here", label: "From this device" }, { value: "everywhere", label: "From the sync folder too" }],
    });
    if (choice === "here") await this.feature.remove(book);
    if (choice === "everywhere") await this.feature.removeEverywhere(book);
  }

  async #removeRemote(name) {
    const ok = await confirmAction({ title: `Delete “${remoteTitle(name)}” from the sync folder?`, message: "Devices that already have it keep their copy.", confirm: "Delete" });
    if (ok) await this.feature.removeRemote(name);
  }

  async #dissolve(group) {
    const ok = await confirmAction({ title: `Dissolve “${group.name}”?`, message: "The books stay on the shelf, on their own.", confirm: "Dissolve" });
    if (ok) await this.feature.dissolve(group);
  }

  #render() {
    const { books, cloudBooks, groups, isAdding, message } = this.feature;
    const sections = groups
      .filter((group) => this.feature.booksIn(group.id).length)
      .map((group) => h("section", {},
        h("h2", {}, group.name, " ", button("Dissolve", () => this.#dissolve(group), { class: "link" })),
        h("ul", { class: "books" }, this.feature.booksIn(group.id).map((book) => this.#row(book)))));
    const loose = this.feature.booksIn(0);
    if (loose.length) sections.push(h("section", {}, sections.length ? h("h2", {}, "No group") : null, h("ul", { class: "books" }, loose.map((book) => this.#row(book)))));
    if (cloudBooks.length) sections.push(h("section", {}, h("h2", {}, "In the sync folder"), h("ul", { class: "books" }, cloudBooks.map((name) => this.#cloudRow(name)))));
    this.setBody(
      isAdding ? note("Adding…") : null,
      message ? h("p", { class: "error message" }, message) : null,
      sections.length ? sections : note("No books yet. Choose Add… to pick an .epub or a .pdf, or drop one here. Books in your sync folder are listed here to download."),
    );
  }
}
