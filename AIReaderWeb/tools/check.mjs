// The command-line check: everything below the views, under Node, against
// the mock and an in-memory library. `node tools/check.mjs [book.epub|book.pdf]`;
// with AIREADER_SYNC_URL set, a WebDAV server too.
import { checkAi } from "./checks/Ai.mjs";
import { checkBooks } from "./checks/Books.mjs";
import { checkCards } from "./checks/Cards.mjs";
import { checkContext } from "./checks/Context.mjs";
import { failures } from "./checks/Check.mjs";
import { checkEpub } from "./checks/Epub.mjs";
import { checkGlossary } from "./checks/Glossary.mjs";
import { checkOffline } from "./checks/Offline.mjs";
import { checkPdf } from "./checks/Pdf.mjs";
import { checkRelay } from "./checks/Relay.mjs";
import { checkServer } from "./checks/Server.mjs";
import { checkStores } from "./checks/Stores.mjs";
import { checkSync } from "./checks/Sync.mjs";
import { openAsBlob } from "node:fs";

// A PDF is checked as the EPUB the library makes of it; the checks that
// take a book take only an EPUB.
const given = process.argv[2];
const pdfPath = /\.pdf$/i.test(given ?? "") ? given : undefined;
const bookPath = pdfPath ? undefined : given;
checkBooks();
await checkAi();
await checkContext();
checkCards();
await checkSync();
await checkStores();
await checkGlossary();
await checkEpub(bookPath);
await checkPdf(pdfPath);
await checkServer(bookPath ? await openAsBlob(bookPath) : null);
await checkRelay();
await checkOffline();
console.log(failures ? "FAILED" : "all checks passed");
process.exitCode = failures ? 1 : 0;
