// A PDF's pages as PDF.js draws them, for a PDF book's page view: the
// library, kept in vendor/ and loaded the first time a page view opens, and
// the document opened from the bytes the shelf kept. What only some PDFs
// need — the standard fonts' data, the CMaps of Chinese, Japanese and Korean
// fonts, the decoders of JPEG 2000 and JBIG2 images — comes from jsDelivr
// when a page needs it.
const version = "5.4.624";
const cdn = `https://cdn.jsdelivr.net/npm/pdfjs-dist@${version}/`;

let loading = null;

/** The PDF.js module, loaded once. */
export function pdfjs() {
  loading ??= import("../../vendor/pdfjs/pdf.min.mjs").then((module) => {
    module.GlobalWorkerOptions.workerSrc = new URL("../../vendor/pdfjs/pdf.worker.min.mjs", import.meta.url).href;
    return module;
  }, (error) => {
    // A module that failed to load, offline say, is asked for again next time.
    loading = null;
    throw error;
  });
  return loading;
}

/** The PDF.js document for the PDF's bytes. Throws why it cannot be opened. */
export async function openPdf(bytes) {
  const { getDocument } = await pdfjs();
  return getDocument({
    // PDF.js takes the buffer over to its worker; a copy leaves the caller's.
    data: bytes.slice(),
    cMapUrl: `${cdn}cmaps/`,
    cMapPacked: true,
    standardFontDataUrl: `${cdn}standard_fonts/`,
    wasmUrl: `${cdn}wasm/`,
    iccUrl: `${cdn}iccs/`,
  }).promise;
}
