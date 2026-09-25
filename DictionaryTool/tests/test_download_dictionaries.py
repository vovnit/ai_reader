import gzip
import json
import os
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import download_dictionaries
from download_dictionaries import (
    DownloadError,
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
