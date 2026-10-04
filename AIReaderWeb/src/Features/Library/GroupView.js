// Which group a book is in. A series, an author, a course: books in one
// group are searched together.
import { askText } from "../Common/Dialogs.js";
import { Screen } from "../Common/Screen.js";
import { button, h, note } from "../Common/Ui.js";

export class GroupView extends Screen {
  constructor(navigator, library, book) {
    super(navigator, "Group");
    const choose = async (group) => {
      await library.assign(book, group);
      navigator.pop(this);
    };
    const option = (label, group, current) => h("li", {}, button(label, () => choose(group), { "aria-pressed": current ? "true" : "false" }));
    this.setBody(
      note(`For “${book.title}”. A series, an author, a course: books in one group are searched together.`),
      h("ul", { class: "choices" },
        option("No group", 0, !book.groupId),
        library.groups.map((group) => option(group.name, group.id, group.id === book.groupId)),
        h("li", {}, button("New group…", async () => {
          const name = await askText({ title: "New group", label: "Name", confirm: "Create" });
          if (name) await choose(name);
        }))),
    );
  }
}
