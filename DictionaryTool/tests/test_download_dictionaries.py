import gzip
import json
import os
import tempfile
import threading
import unittest
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from unittest.mock import patch

import download_dictionaries
from download_dictionaries import (
    DownloadError,
    download_file,
    install_lexique,
    install_wikidict_ru,
    install_wiktionary,
    safe_extract,
)


LEXIQUE_CONTENT = (
    "1_Mot\t2_Phono\t3_Phono_IPA\t4_Lemme\t5_Cgram\t6_CgramOrtho\t"
    "7_Genre\t8_Nombre\t9_InfoVER\n"
    "mange\tmɑ̃ʒ\tmɑ̃ʒ\tmanger\tVER\tVER\t\ts\tind:pre:3\n"
)
PAYLOAD = bytes(range(256)) * 1200


class CutOffServer(BaseHTTPRequestHandler):
    """Sends the first response short of its Content-Length, as a dropped
    connection does, and answers a Range request with the rest unless
    `honors_range` is off. A `first_status` answers the first request with
    that error instead."""

    honors_range = True
    first_status = None
    requests = []

    def do_GET(self):
        self.requests.append(dict(self.headers))
        if self.first_status and len(self.requests) == 1:
            self.send_error(self.first_status)
            return
        requested = self.headers.get("Range")
        if requested and self.honors_range:
            start = int(requested[len("bytes="):-1])
            self.send_response(206)
            self.send_header("Content-Range", "bytes {}-{}/{}".format(start, len(PAYLOAD) - 1, len(PAYLOAD)))
            self.send_header("Content-Length", str(len(PAYLOAD) - start))
            self.end_headers()
            self.wfile.write(PAYLOAD[start:])
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(PAYLOAD)))
        self.send_header("ETag", '"v1"')
        self.end_headers()
        self.wfile.write(PAYLOAD[: len(PAYLOAD) // 3] if len(self.requests) == 1 else PAYLOAD)

    def log_message(self, *arguments):
        pass


class DownloaderTests(unittest.TestCase):
    def test_rejects_archive_path_traversal(self):
        with tempfile.TemporaryDirectory() as directory:
            archive_path = Path(directory) / "unsafe.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("../outside.txt", "unsafe")

            with zipfile.ZipFile(archive_path) as archive:
                with self.assertRaisesRegex(DownloadError, "unsafe path"):
                    safe_extract(archive, Path(directory) / "output")

    def test_installs_downloads_from_staging_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source_archive = root / "source.zip"
            source_jsonl = root / "source.jsonl"
            source_jsonl_gzip = root / "source.jsonl.gz"
            destination = root / "data"
            destination.mkdir()
            with zipfile.ZipFile(source_archive, "w") as archive:
                archive.writestr("release/Lexique4.tsv", LEXIQUE_CONTENT)
                archive.writestr("release/README.txt", "Lexique fixture")
            source_jsonl.write_text(
                json.dumps({"title": "Manger", "redirect": "manger"})
                + "\n"
                + json.dumps({"word": "manger", "lang_code": "fr"})
                + "\n",
                encoding="utf-8",
            )
            with source_jsonl.open("rb") as source, gzip.open(
                source_jsonl_gzip, "wb"
            ) as destination_file:
                destination_file.write(source.read())

            messages = []
            with patch.object(download_dictionaries, "LEXIQUE_URL", source_archive.as_uri()):
                install_lexique(destination, reporter=messages.append)
            with patch.object(
                download_dictionaries,
                "WIKTIONARY_URL_TEMPLATE",
                source_jsonl_gzip.as_uri(),
            ):
                install_wiktionary(
                    destination,
                    definition_language="ru",
                    reporter=messages.append,
                )

            self.assertTrue((destination / "Lexique4" / "Lexique4.tsv").is_file())
            self.assertTrue((destination / "ru-extract.jsonl").is_file())
            self.assertTrue(
                (destination / "ru-extract.jsonl.metadata.json").is_file()
            )
            self.assertTrue(any("Installed Lexique4" in message for message in messages))

    def test_english_edition_downloads_from_its_own_url(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "English.jsonl.gz"
            with gzip.open(source, "wt", encoding="utf-8") as output:
                output.write(json.dumps({"pos": "noun", "word": "cat", "lang_code": "en"}) + "\n")

            with patch.dict(
                download_dictionaries.WIKTIONARY_URLS, {"en": source.as_uri()}
            ), patch.object(
                download_dictionaries,
                "WIKTIONARY_URL_TEMPLATE",
                (root / "missing-{language}.gz").as_uri(),
            ):
                install_wiktionary(root, definition_language="en", reporter=lambda message: None)

            self.assertTrue((root / "en-extract.jsonl").is_file())
            metadata = json.loads((root / "en-extract.jsonl.metadata.json").read_text())
            self.assertEqual(metadata["source_url"], source.as_uri())
        self.assertEqual(
            download_dictionaries.WIKTIONARY_URLS["en"],
            "https://kaikki.org/dictionary/English/kaikki.org-dictionary-English.jsonl.gz",
        )

    def serve(self, **behaviour):
        handler = type("Handler", (CutOffServer,), {"requests": [], **behaviour})
        server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        pause = patch.object(download_dictionaries, "RETRY_PAUSE", 0)
        pause.start()
        self.addCleanup(pause.stop)
        return "http://127.0.0.1:{}/file.gz".format(server.server_port), handler.requests

    def download_from_cut_off_server(self, **behaviour):
        url, requests = self.serve(**behaviour)
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "file.gz"
            messages = []
            download_file(url, destination, messages.append)
            self.assertEqual(destination.read_bytes(), PAYLOAD)
            self.assertEqual(os.listdir(directory), ["file.gz"])
        self.assertTrue(any("resuming" in message for message in messages))
        return requests

    def test_download_resumes_where_it_was_cut_off(self):
        requests = self.download_from_cut_off_server(honors_range=True)

        self.assertEqual(len(requests), 2)
        self.assertEqual(requests[1]["Range"], "bytes={}-".format(len(PAYLOAD) // 3))
        self.assertEqual(requests[1]["If-Range"], '"v1"')

    def test_download_starts_again_when_the_server_ignores_ranges(self):
        requests = self.download_from_cut_off_server(honors_range=False)

        self.assertEqual(len(requests), 2)

    def test_download_tries_again_after_a_server_error(self):
        requests = self.download_from_cut_off_server(first_status=503)

        self.assertEqual(len(requests), 2)

    def test_download_stops_at_a_missing_file(self):
        url, requests = self.serve(first_status=404)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(DownloadError, "404"):
                download_file(url, Path(directory) / "file.gz", lambda message: None)
            self.assertEqual(os.listdir(directory), [])
        self.assertEqual(len(requests), 1)

    def test_download_gives_up_after_its_attempts(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(
            download_dictionaries, "RETRY_PAUSE", 0
        ):
            destination = Path(directory) / "file.gz"
            with self.assertRaisesRegex(DownloadError, "2 attempts"):
                download_file(
                    "http://127.0.0.1:9/file.gz", destination, lambda message: None, attempts=2
                )
            self.assertEqual(os.listdir(directory), [])

    def test_main_uses_explicit_data_directory_from_any_working_directory(self):
        with tempfile.TemporaryDirectory() as directory, tempfile.TemporaryDirectory() as elsewhere:
            data_dir = Path(directory)
            lexique_dir = data_dir / "Lexique4"
            lexique_dir.mkdir()
            (lexique_dir / "Lexique4.tsv").write_text(LEXIQUE_CONTENT, encoding="utf-8")
            (data_dir / "fr-extract.jsonl").write_text(
                json.dumps({"word": "manger", "lang_code": "fr"}) + "\n",
                encoding="utf-8",
            )
            previous_directory = Path.cwd()
            try:
                os.chdir(elsewhere)
                status = download_dictionaries.main(["--data-dir", str(data_dir)])
            finally:
                os.chdir(previous_directory)

            self.assertEqual(status, 0)
            self.assertTrue((data_dir / "Lexique4" / "Lexique4.tsv").is_file())

    def test_installs_wikidict_pair_from_staging_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.txt"
            source.write_text("France\tФранция\nParis\tПариж\n", encoding="utf-8")
            messages = []

            with patch.object(
                download_dictionaries,
                "WIKIDICT_RU_URL_TEMPLATE",
                source.as_uri(),
            ):
                install_wikidict_ru(root / "data", "fr", reporter=messages.append)

            destination = root / "data" / "wikidict-ru" / "fr-ru_wiki.txt"
            self.assertEqual(
                destination.read_text(encoding="utf-8"),
                source.read_text(encoding="utf-8"),
            )
            self.assertTrue(
                any("Validated fr -> ru" in message for message in messages)
            )


if __name__ == "__main__":
    unittest.main()
