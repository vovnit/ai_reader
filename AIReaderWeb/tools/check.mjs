// The command-line check: everything below the views, under Node, against
// the mock and an in-memory library. `node tools/check.mjs [book]`, the book
// an EPUB or any other file the library takes in; with AIREADER_SYNC_URL
// set, a WebDAV server too.
import { checkAi } from "./checks/Ai.mjs";
import { checkBooks } from "./checks/Books.mjs";
import { checkCards } from "./checks/Cards.mjs";
import { checkContext } from "./checks/Context.mjs";
import { failures } from "./checks/Check.mjs";
import { checkCharset } from "./checks/Charset.mjs";
import { checkEpub } from "./checks/Epub.mjs";
import { checkFb2 } from "./checks/Fb2.mjs";
import { checkGlossary } from "./checks/Glossary.mjs";
import { checkHtmlBook } from "./checks/HtmlBook.mjs";
import { checkImport } from "./checks/Import.mjs";
import { checkOffline } from "./checks/Offline.mjs";
import { checkPdf } from "./checks/Pdf.mjs";
import { checkPhoto } from "./checks/Photo.mjs";
import { checkRelay } from "./checks/Relay.mjs";
import { checkRecordsServer, checkServer } from "./checks/Server.mjs";
import { checkStores } from "./checks/Stores.mjs";
import { checkSync, checkSyncParts } from "./checks/Sync.mjs";
import { checkTextBook } from "./checks/TextBook.mjs";
import { bookFormat } from "../src/Services/BookImporter.js";
import { openAsBlob } from "node:fs";

// A PDF, FB2, text or HTML book is checked as the EPUB the library makes of
// it; the checks that take a book take only an EPUB.
const given = process.argv[2];
const { format } = bookFormat(given ?? "");
const pdfPath = format === "pdf" ? given : undefined;
const importPath = ["fb2", "fb2.zip", "txt", "html"].includes(format) ? given : undefined;
const bookPath = pdfPath || importPath ? undefined : given;
checkBooks();
await checkAi();
await checkContext();
checkCards();
await checkSync();
checkSyncParts();
await checkStores();
await checkGlossary();
await checkEpub(bookPath);
checkCharset();
checkFb2();
checkTextBook();
checkHtmlBook();
await checkPdf(pdfPath);
await checkImport(importPath);
await checkPhoto();
await checkServer(bookPath ? await openAsBlob(bookPath) : null);
await checkRecordsServer();
await checkRelay();
await checkOffline();
console.log(failures ? "FAILED" : "all checks passed");
process.exitCode = failures ? 1 : 0;
