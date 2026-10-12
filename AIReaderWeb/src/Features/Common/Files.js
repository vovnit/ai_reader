// Files in and out of the page: the system's file picker or a phone's
// camera, and a download. A file input is kept in the page while it is
// open: on a phone the picker or the camera can outlive one that is not,
// and the file it was to hand over is lost.

/** The files the reader picked; none when they cancelled. */
export function pickFiles({ accept = "", multiple = true } = {}) {
  return new Promise((resolve) => {
    const input = fileInput({ accept, multiple });
    const done = (files) => {
      input.remove();
      resolve(files);
    };
    input.addEventListener("change", () => done([...input.files]));
    input.addEventListener("cancel", () => done([]));
    document.body.append(input);
    input.click();
  });
}

/** A hidden file input to put in the page. `capture` "environment" opens a phone's back camera instead of the picker. */
export function fileInput({ accept = "", multiple = false, capture = "" } = {}) {
  const input = document.createElement("input");
  input.type = "file";
  input.accept = accept;
  input.multiple = multiple;
  input.hidden = true;
  // Where there is no camera to open, the picker is shown as ever.
  if (capture) input.setAttribute("capture", capture);
  return input;
}

export function saveFile(name, blob) {
  const url = URL.createObjectURL(blob);
  const link = Object.assign(document.createElement("a"), { href: url, download: name });
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}
