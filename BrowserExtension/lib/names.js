// The file name a book is given in the shared `Books` folder: author and
// title, with nothing a Kindle's FAT partition would refuse. The same rule
// as the apps' RemoteBookName (Swift and C++).

function stem(title, author) {
  const trimmedAuthor = (author ?? "").trim();
  const raw = trimmedAuthor ? `${trimmedAuthor} - ${title}` : title;
  let stem = raw.replace(/[\/\\:*?"<>|\u0000-\u001f\u007f]/g, " ").split(/\s+/).filter(Boolean).join(" ");
  stem = Array.from(stem).slice(0, 120).join("");
  // Windows and FAT drop a trailing dot, and a leading one hides the file.
  stem = stem.replace(/[. ]+$/, "").replace(/^[. ]+/, "");
  return stem || "Book";
}

/** A name none of `taken` has, compared without case. */
export function remoteBookName(title, author, taken) {
  const lowered = new Set(taken.map((name) => name.toLowerCase()));
  const base = stem(title, author);
  let name = `${base}.epub`;
  for (let number = 2; lowered.has(name.toLowerCase()); number++) name = `${base} (${number}).epub`;
  return name;
}
