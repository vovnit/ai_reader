// The file name a book is given in the sync folder's `Books`: author and
// title, so it reads well in any file manager, with nothing a Kindle's FAT
// partition would refuse. The same rule as `RemoteBookName.swift`,
// `RemoteBookName.cpp` and the browser extension's `lib/names.js`.

export const booksFolder = "Books";

function stem(title, author) {
  const trimmedAuthor = (author ?? "").trim();
  const raw = trimmedAuthor ? `${trimmedAuthor} - ${title}` : title;
  // Forbidden and control characters become spaces, and runs of spaces one.
  let cleaned = raw.replace(/[\/\\:*?"<>| \u0000-\u001f\u007f]+/g, " ").replace(/^ /, "");
  cleaned = Array.from(cleaned).slice(0, 120).join("");
  // Windows and FAT drop a trailing dot, and a leading one hides the file.
  cleaned = cleaned.replace(/[. ]+$/, "").replace(/^[. ]+/, "");
  return cleaned || "Book";
}

/** A name none of `taken` has, compared without case, since some servers ignore it. */
export function remoteBookName(title, author, taken) {
  const lowered = new Set(taken.map((name) => name.toLowerCase()));
  const base = stem(title, author);
  let name = `${base}.epub`;
  for (let number = 2; lowered.has(name.toLowerCase()); number++) name = `${base} (${number}).epub`;
  return name;
}

export function isRemoteBook(name) {
  return name.toLowerCase().endsWith(".epub") && !name.startsWith(".");
}
