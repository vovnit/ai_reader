# Tesseract.js

The text reader behind the web app's photo lookup:
[Tesseract.js](https://github.com/naptha/tesseract.js) 7.0.0 and the SIMD,
LSTM-only build of its core, [tesseract.js-core](https://github.com/naptha/tesseract.js-core)
7.0.0 (Tesseract compiled to WebAssembly), copied unchanged from their npm
packages:

| File | Package |
| --- | --- |
| `tesseract.esm.min.js`, `tesseract.min.js.LICENSE.txt` | `tesseract.js/dist` |
| `worker.min.js`, `worker.min.js.LICENSE.txt` | `tesseract.js/dist` |
| `tesseract-core-simd-lstm.js`, `tesseract-core-simd-lstm.wasm` | `tesseract.js-core` |

Both are under the Apache License 2.0 (`LICENSE.md`); the `.LICENSE.txt`
files name the libraries bundled into the two scripts. Every browser the app
runs in has WebAssembly SIMD, so the other builds of the core are left out.

The language models are not here: the worker fetches each one from jsDelivr
(`@tesseract.js-data/<language>/4.0.0_best_int`, 1–3 MB) the first time a
photo needs it, and keeps it in the browser's IndexedDB.

To update, take the same files from the new versions' packages
(`npm pack tesseract.js tesseract.js-core`, the core at the version the new
`tesseract.js` depends on), then run `../../build.sh check` and photograph a
page.
