// Files in and out of the page: the system's file picker or a phone's
// camera, and a download.

/** The files the reader picked; none when they cancelled. `capture` "environment" opens a phone's back camera instead. */
export function pickFiles({ accept = "", multiple = true, capture = "" } = {}) {
  return new Promise((resolve) => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = accept;
    input.multiple = multiple;
    // Where there is no camera to open, the picker is shown as ever.
    if (capture) input.setAttribute("capture", capture);
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
