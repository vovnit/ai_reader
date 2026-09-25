import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from definition_lookup import DefinitionLookup
from dictionary_data import DictionaryDataError
import lookup as lookup_module
from lookup import DictionaryLookup, main


LEXIQUE_HEADER = (
    "1_Mot\t2_Phono\t3_Phono_IPA\t4_Lemme\t5_Cgram\t6_CgramOrtho\t"
    "7_Genre\t8_Nombre\t9_InfoVER\n"
)


class LookupTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.data_dir = Path(self.temporary_directory.name)
        self.lexique_path = self.data_dir / "Lexique4.tsv"
        self.wiktionary_path = self.data_dir / "fr-extract.jsonl"
        self.index_path = self.data_dir / "index.sqlite3"
        self.lexique_path.write_text(
            LEXIQUE_HEADER
            + "mangent\tmɑ̃ʒ\tmɑ̃ʒ\tmanger\tVER\tVER\t\tp\tind:pre:3,ind:pre:2\n"
            + "chevaux\tʃəvo\tʃəvo\tcheval\tNOM\tNOM\tm\tp\t\n",
            encoding="utf-8",
        )
        records = [
            {
                "word": "manger",
                "lang_code": "fr",
                "pos": "verb",
                "pos_title": "Verbe",
                "etymology_texts": ["Du latin manducare."],
                "senses": [
                    {"glosses": ["Mâcher puis avaler un aliment."]},
                    {"tags": ["obsolete"]},
                ],
            },
            {
                "word": "manger",
                "lang_code": "en",
                "pos": "noun",
                "senses": [{"glosses": ["A trough."]}],
            },
            {
                "word": "bonjour",
                "lang_code": "fr",
                "pos": "intj",
                "pos_title": "Interjection",
                "senses": [{"glosses": ["Salutation employée pendant la journée."]}],
            },
            {
                "word": "Haus",
                "lang_code": "de",
                "pos": "noun",
                "pos_title": "Nom commun",
                "senses": [{"glosses": ["Maison."]}],
            },
        ]
        self.wiktionary_path.write_text(
            "".join(json.dumps(record, ensure_ascii=False) + "\n" for record in records),
            encoding="utf-8",
        )

    def tearDown(self):
        self.temporary_directory.cleanup()

    def make_lookup(self, reporter=None):
        return DictionaryLookup(
            self.lexique_path,
            self.wiktionary_path,
            self.index_path,
            reporter=reporter,
        )

    def test_resolves_form_to_lemma_and_definitions(self):
        messages = []

        result = self.make_lookup(messages.append).lookup("  MANGENT ! ")

        self.assertEqual(result["normalized"], "mangent")
        self.assertTrue(result["found"])
        self.assertEqual([lemma["lemma"] for lemma in result["lemmas"]], ["manger"])
        form = result["lemmas"][0]["forms"][0]
        self.assertEqual(form["part_of_speech"], "VER")
        self.assertEqual(form["verb_info"], ["ind:pre:3", "ind:pre:2"])
        entry = result["lemmas"][0]["entries"][0]
        self.assertEqual(entry["part_of_speech"], "verb")
        self.assertEqual(entry["definitions"], [{"glosses": ["Mâcher puis avaler un aliment."]}])
        self.assertTrue(any("Definition index ready" in message for message in messages))

    def test_looks_up_direct_lemma_when_form_is_absent(self):
        result = self.make_lookup().lookup("Bonjour")

        self.assertTrue(result["found"])
        self.assertEqual(result["lemmas"][0]["lemma"], "bonjour")
        self.assertEqual(result["lemmas"][0]["forms"], [])
        self.assertEqual(
            result["lemmas"][0]["entries"][0]["definitions"][0]["glosses"][0],
            "Salutation employée pendant la journée.",
        )

    def test_invalid_input_does_not_touch_dictionary_files(self):
        result = self.make_lookup().lookup("42")

        self.assertEqual(
            result,
            {
                "query": "42",
                "normalized": "42",
                "target_language": "fr",
                "definition_language": "fr",
                "valid": False,
                "found": False,
                "lemmas": [],
            },
        )
        self.assertFalse(self.index_path.exists())

    def test_definition_index_rebuilds_when_source_changes(self):
        definitions = DefinitionLookup(self.wiktionary_path, self.index_path)
        self.assertEqual(len(definitions.find(["manger"])["manger"]), 1)
        with self.wiktionary_path.open("a", encoding="utf-8") as destination:
            destination.write(
                json.dumps(
                    {
                        "word": "cheval",
                        "lang_code": "fr",
                        "pos": "noun",
                        "senses": [{"glosses": ["Grand mammifère domestiqué."]}],
                    },
                    ensure_ascii=False,
                )
                + "\n"
            )

        self.assertEqual(len(definitions.find(["cheval"])["cheval"]), 1)

    def test_selects_target_language_from_definition_source(self):
        lookup = DictionaryLookup(
            self.lexique_path,
            self.wiktionary_path,
            self.data_dir / "de-index.sqlite3",
            target_language="de",
            definition_language="fr",
        )

        result = lookup.lookup("Haus")

        self.assertTrue(result["found"])
        self.assertEqual(result["target_language"], "de")
        self.assertEqual(result["definition_language"], "fr")
        self.assertEqual(result["lemmas"][0]["forms"], [])
        self.assertEqual(
            result["lemmas"][0]["entries"][0]["definitions"][0]["glosses"],
            ["Maison."],
        )

    def test_definition_language_loads_matching_extract(self):
        def install_russian(data_dir, definition_language, reporter):
            self.assertEqual(definition_language, "ru")
            reporter("Installed test Russian extract")
            (Path(data_dir) / "ru-extract.jsonl").write_text(
                json.dumps(
                    {
                        "word": "manger",
                        "lang_code": "fr",
                        "pos": "verb",
                        "pos_title": "Глагол",
                        "senses": [{"glosses": ["Есть, принимать пищу."]}],
                    },
                    ensure_ascii=False,
                )
                + "\n",
                encoding="utf-8",
            )

        with patch.object(lookup_module, "BASE_DIR", self.data_dir), patch(
            "download_dictionaries.install_wiktionary",
            side_effect=install_russian,
        ) as installer:
            lookup = DictionaryLookup(
                self.lexique_path,
                None,
                self.data_dir / "ru-index.sqlite3",
                target_language="fr",
                definition_language="ru",
            )
            result = lookup.lookup("mangent")

        installer.assert_called_once()
        self.assertEqual(result["definition_language"], "ru")
        self.assertEqual(
            result["lemmas"][0]["entries"][0]["definitions"][0]["glosses"],
            ["Есть, принимать пищу."],
        )

    def test_rejects_mislabeled_definition_source(self):
        with self.assertRaisesRegex(DictionaryDataError, "fr definitions, not ru"):
            DictionaryLookup(
                self.lexique_path,
                self.wiktionary_path,
                self.data_dir / "wrong-index.sqlite3",
                target_language="fr",
                definition_language="ru",
                download_missing=False,
            )

    def test_cli_prints_only_json_to_stdout(self):
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = main(
                [
                    "mangent",
                    "--lexique",
                    str(self.lexique_path),
                    "--wiktionary",
                    str(self.wiktionary_path),
                    "--index",
                    str(self.index_path),
                    "--compact",
                ]
            )

        self.assertEqual(status, 0)
        self.assertEqual(json.loads(stdout.getvalue())["lemmas"][0]["lemma"], "manger")
        self.assertNotIn("Building", stdout.getvalue())


if __name__ == "__main__":
    unittest.main()
