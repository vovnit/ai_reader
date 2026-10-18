# PDF.js

What draws a PDF book's original pages in the web app's page view:
[PDF.js](https://github.com/mozilla/pdf.js) 5.4.624, its two scripts copied
unchanged from the `pdfjs-dist` npm package:

| File | Package |
| --- | --- |
| `pdf.min.mjs` | `pdfjs-dist/build` |
| `pdf.worker.min.mjs` | `pdfjs-dist/build` |

It is under the Apache License 2.0 (`LICENSE`).

What only some PDFs need is not here: the standard fonts' data (for PDFs that
do not embed Times or Helvetica), the CMaps of Chinese, Japanese and Korean
fonts, and the decoders of JPEG 2000 and JBIG2 images. PDF.js fetches each
from jsDelivr (`pdfjs-dist@5.4.624`) the first time a page needs it.

To update, take the same two files from the new version's package
(`npm pack pdfjs-dist`), change the version where the page view names the
jsDelivr address, then run `../../build.sh check` and open a PDF book's pages.
