import unittest

from form_lookup import FormMatch
from lemma_extraction import extract_lemmas
from normalization import normalize
from structured_output import build_result, empty_result


class StageTests(unittest.TestCase):
    def test_lemma_extraction_deduplicates_lexique_matches(self):
        matches = [
            FormMatch("lire", "lire", "VER", None, "s", ["inf"]),
            FormMatch("lire", "lire", "NOM", "f", "s", []),
        ]

        self.assertEqual(extract_lemmas("lire", matches), ["lire"])

    def test_lemma_extraction_falls_back_to_normalized_word(self):
        self.assertEqual(extract_lemmas("bonjour", []), ["bonjour"])

    def test_structured_output_combines_stage_results(self):
        normalized_word = normalize("Mangent")
        match = FormMatch(
            "mangent", "manger", "VER", None, "p", ["ind:pre:3"]
        )

        result = build_result(
            normalized_word,
            [match],
            ["manger"],
            {"manger": [{"part_of_speech": "verb", "definitions": []}]},
            "fr",
            "fr",
        )

        self.assertTrue(result["found"])
        self.assertEqual(result["lemmas"][0]["lemma"], "manger")
        self.assertEqual(result["lemmas"][0]["forms"][0]["number"], "p")

    def test_empty_output_preserves_invalid_normalization(self):
        self.assertEqual(
            empty_result(normalize("42"), "fr", "fr"),
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


if __name__ == "__main__":
    unittest.main()
