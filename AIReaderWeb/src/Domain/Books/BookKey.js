// The name a book goes by across devices, since neither the record's id
// nor the file is the same on two of them: its title and author, lowercased
// and with runs of spaces, tabs and line breaks made one space. The same
// rule as `BookKey.swift` and `BookKey.cpp`.

function normalize(value) {
  return value.toLowerCase().split(/[ \t\n]+/).filter(Boolean).join(" ");
}

export function bookKey(title, author) {
  return `${normalize(title ?? "")}|${normalize(author ?? "")}`;
}
