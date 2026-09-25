// A page as the extractor returned it, made self-contained: its images
// fetched and brought to what an e-reader shows, and the markup pointed at
// the copies. Runs in the popup, which may fetch from any site it has been
// given access to.

const maxSide = 1600;
const readable = { "image/jpeg": "jpg", "image/png": "png", "image/gif": "gif" };

async function decode(blob) {
  const url = URL.createObjectURL(blob);
  try {
    const image = new Image();
    image.src = url;
    await image.decode();
    return image;
  } finally {
    URL.revokeObjectURL(url);
  }
}

/**
 * The image as JPEG, PNG or GIF, at most `maxSide` on its longer side:
 * WebP, AVIF and SVG are redrawn, since a Kindle cannot show them.
 */
async function fetchImage(source) {
  const response = await fetch(source, { credentials: "include" });
  if (!response.ok) throw new Error(`image answered ${response.status}`);
  const blob = await response.blob();
  const type = blob.type.split(";")[0].trim();
  const image = await decode(blob);
  const width = image.naturalWidth || maxSide;
  const height = image.naturalHeight || maxSide;
  if (readable[type] && Math.max(width, height) <= maxSide) {
    return { type, bytes: new Uint8Array(await blob.arrayBuffer()) };
  }

  const scale = Math.min(1, maxSide / Math.max(width, height));
  const canvas = document.createElement("canvas");
  canvas.width = Math.max(1, Math.round(width * scale));
  canvas.height = Math.max(1, Math.round(height * scale));
  const context = canvas.getContext("2d");
  // Transparency is kept for PNG and SVG, which use it for diagrams; a
  // photograph becomes a JPEG on white.
  const transparent = type === "image/png" || type === "image/svg+xml";
  if (!transparent) {
    context.fillStyle = "#fff";
    context.fillRect(0, 0, canvas.width, canvas.height);
  }
  context.drawImage(image, 0, 0, canvas.width, canvas.height);
  const outputType = transparent ? "image/png" : "image/jpeg";
  const output = await new Promise((resolve) => canvas.toBlob(resolve, outputType, 0.85));
  if (!output) throw new Error("image could not be redrawn");
  return { type: outputType, bytes: new Uint8Array(await output.arrayBuffer()) };
}

/**
 * The page with its images: `{ title, byline, site, language, url, xhtml,
 * images }`, each image `{ path, type, bytes }`. `id` keeps the image
 * names of one page apart from another's in the same book. An image that
 * cannot be fetched is left out.
 */
export async function capturePage(extracted, id) {
  const fetched = new Map();
  const queue = [...extracted.images];
  const worker = async () => {
    while (queue.length > 0) {
      const source = queue.shift();
      try {
        const image = await fetchImage(source);
        const path = `images/${id}-${fetched.size + 1}.${readable[image.type]}`;
        fetched.set(source, { path, ...image });
      } catch (error) {
        console.warn("AIReader: left out", source, error);
      }
    }
  };
  await Promise.all(Array.from({ length: 4 }, worker));

  const document = new DOMParser().parseFromString(extracted.xhtml, "application/xhtml+xml");
  for (const image of [...document.getElementsByTagName("img")]) {
    const copy = fetched.get(image.getAttribute("src"));
    if (copy) image.setAttribute("src", `../${copy.path}`);
    else image.remove();
  }
  const { images: _, ...page } = extracted;
  return {
    ...page,
    xhtml: new XMLSerializer().serializeToString(document.documentElement),
    images: [...fetched.values()],
  };
}
