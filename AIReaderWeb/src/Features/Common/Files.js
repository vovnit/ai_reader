// Files in and out of the page: the system's file picker, and a download.

/** The files the reader picked; none when they cancelled. */
export function pickFiles({ accept = "", multiple = true } = {}) {
  return new Promise((resolve) => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = accept;
    input.multiple = multiple;
    input.addEventListener("change", () => resolve([...input.files]));
    input.addEventListener("cancel", () => resolve([]));
    input.click();
  });
}

export function saveFile(name, blob) {
  const url = URL.createObjectURL(blob);
  const link = Object.assign(document.createElement("a"), { href: url, download: name });
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}
