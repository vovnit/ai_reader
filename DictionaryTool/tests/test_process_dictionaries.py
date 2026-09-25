import json
import tempfile
import unittest
from pathlib import Path

from dictionary_data import DictionaryDataError
from lookup import DictionaryLookup
from process_dictionaries import ProcessingError, build_processed_dictionary


LEXIQUE_HEADER = (
    "1_Mot\t2_Phono\t3_Phono_IPA\t4_Lemme\t5_Cgram\t6_CgramOrtho\t"
    "7_Genre\t8_Nombre\t9_InfoVER\n"
)


class ProcessDictionaryTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.data_dir = Path(self.temporary_directory.name)
        self.lexique_path = self.data_dir / "Lexique4.tsv"
        self.wiktionary_path = self.data_dir / "fr-extract.jsonl"
        self.source_index_path = self.data_dir / "source-index.sqlite3"
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
                    {"glosses": ["Prendre un repas."], "tags": ["broadly"]},
                ],
                "sounds": [{"audio": "unused.ogg"}],
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
            {
                "word": "manger",
                "lang_code": "en",
                "pos": "noun",
                "senses": [{"glosses": ["A trough."]}],
            },
        ]
        self.wiktionary_path.write_text(
            "".join(json.dumps(record, ensure_ascii=False) + "\n" for record in records),
            encoding="utf-8",
        )

    def tearDown(self):
        self.temporary_directory.cleanup()

    def source_lookup(self):
        return DictionaryLookup(
            self.lexique_path,
            self.wiktionary_path,
            self.source_index_path,
        )

    def test_compact_database_preserves_lexique_reachable_results(self):
        output = self.data_dir / "compact.sqlite3"
        summary = build_processed_dictionary(
            output,
            self.lexique_path,
            self.wiktionary_path,
            reporter=lambda message: None,
        )

        processed = DictionaryLookup(database_path=output)
        self.assertEqual(
            processed.lookup("mangent"),
            self.source_lookup().lookup("mangent"),
        )
        self.assertFalse(processed.lookup("bonjour")["found"])
        self.assertEqual(summary["mode"], "compact")
        self.assertEqual(summary["definition_entries"], 1)

    def test_lossless_database_preserves_direct_lookup_results(self):
        output = self.data_dir / "lossless.sqlite3"
        summary = build_processed_dictionary(
            output,
            self.lexique_path,
            self.wiktionary_path,
            lossless=True,
            compress=False,
            reporter=lambda message: None,
        )

        processed = DictionaryLookup(database_path=output)
        source = self.source_lookup()
        self.assertEqual(processed.lookup("mangent"), source.lookup("mangent"))
        self.assertEqual(processed.lookup("bonjour"), source.lookup("bonjour"))
        self.assertEqual(summary["mode"], "lossless")
        self.assertEqual(summary["definition_entries"], 2)

    def test_non_french_pack_selects_language_and_uses_direct_lookup(self):
        output = self.data_dir / "fr-de.sqlite3"
        summary = build_processed_dictionary(
            output,
            self.lexique_path,
            self.wiktionary_path,
            target_language="de",
            definition_language="fr",
            reporter=lambda message: None,
        )

        processed = DictionaryLookup(database_path=output)
        result = processed.lookup("Haus")

        self.assertTrue(result["found"])
        self.assertEqual(result["target_language"], "de")
        self.assertEqual(result["definition_language"], "fr")
        self.assertEqual(result["lemmas"][0]["forms"], [])
        self.assertEqual(summary["mode"], "lossless")
        self.assertEqual(summary["forms"], 0)
        self.assertEqual(summary["definition_entries"], 1)

    def test_russian_edition_composes_french_target_pack(self):
        russian_source = self.data_dir / "ru-extract.jsonl"
        russian_source.write_text(
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
        output = self.data_dir / "ru-fr.sqlite3"

        build_processed_dictionary(
            output,
            self.lexique_path,
            russian_source,
            target_language="fr",
            definition_language="ru",
            reporter=lambda message: None,
        )
        result = DictionaryLookup(database_path=output).lookup("mangent")

        self.assertEqual(result["target_language"], "fr")
        self.assertEqual(result["definition_language"], "ru")
        self.assertEqual(
            result["lemmas"][0]["entries"][0]["definitions"][0]["glosses"],
            ["Есть, принимать пищу."],
        )

    def test_wikidict_russian_mappings_use_definition_payload_format(self):
        russian_source = self.data_dir / "ru-extract.jsonl"
        russian_source.write_text(
            json.dumps(
                {
                    "word": "manger",
                    "lang_code": "fr",
                    "pos": "verb",
                    "senses": [{"glosses": ["Есть."]}],
                },
                ensure_ascii=False,
            )
            + "\n",
            encoding="utf-8",
        )
        wikidict_source = self.data_dir / "fr-ru_wiki.txt"
        wikidict_source.write_text(
            "Manger\tПотребление пищи\nParis\tПариж\nmalformed\n",
            encoding="utf-8",
        )
        output = self.data_dir / "ru-fr-wikidict.sqlite3"

        summary = build_processed_dictionary(
            output,
            self.lexique_path,
            russian_source,
            target_language="fr",
            definition_language="ru",
            reporter=lambda message: None,
            wikidict_path=wikidict_source,
        )
        result = DictionaryLookup(database_path=output).lookup("mangent")

        self.assertEqual(len(result["lemmas"][0]["entries"]), 2)
        self.assertEqual(
            result["lemmas"][0]["entries"][1],
            {
                "part_of_speech": None,
                "part_of_speech_title": None,
                "etymologies": [],
                "definitions": [{"glosses": ["Потребление пищи"]}],
            },
        )
        self.assertEqual(summary["wikidict_entries"], 1)
        self.assertEqual(summary["malformed_wikidict_rows"], 1)

    def test_pack_builder_rejects_mislabeled_source(self):
        with self.assertRaisesRegex(DictionaryDataError, "fr definitions, not ru"):
            build_processed_dictionary(
                self.data_dir / "wrong.sqlite3",
                self.lexique_path,
                self.wiktionary_path,
                target_language="fr",
                definition_language="ru",
                reporter=lambda message: None,
            )

    def test_refuses_to_replace_output_without_force(self):
        output = self.data_dir / "existing.sqlite3"
        output.write_bytes(b"existing")

        with self.assertRaisesRegex(ProcessingError, "--force"):
            build_processed_dictionary(
                output,
                self.lexique_path,
                self.wiktionary_path,
                reporter=lambda message: None,
            )

    def test_never_replaces_a_source_dictionary(self):
        with self.assertRaisesRegex(ProcessingError, "source dictionary"):
            build_processed_dictionary(
                self.lexique_path,
                self.lexique_path,
                self.wiktionary_path,
                force=True,
                reporter=lambda message: None,
            )


if __name__ == "__main__":
    unittest.main()
