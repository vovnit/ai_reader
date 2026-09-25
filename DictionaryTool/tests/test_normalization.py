import unicodedata
import unittest

from normalization import is_valid_word_form, normalize


class NormalizationTests(unittest.TestCase):
    def test_normalizes_french_punctuation_case_and_unicode(self):
        result = normalize("  « L’HOMME. » ")

        self.assertEqual(result.original, "  « L’HOMME. » ")
        self.assertEqual(result.normalized, "l'homme")
        self.assertTrue(result.is_valid)

    def test_normalizes_decomposed_accents_and_hyphens(self):
        word = unicodedata.normalize("NFD", "ARRIÈRE‑PENSÉE")

        self.assertEqual(normalize(word).normalized, "arrière-pensée")

    def test_rejects_non_word_and_repeated_separators(self):
        numeric = normalize("!123!")
        self.assertEqual(numeric.normalized, "123")
        self.assertFalse(numeric.is_valid)
        self.assertFalse(normalize("123chat").is_valid)
        self.assertFalse(is_valid_word_form("porte--monnaie"))
        self.assertFalse(is_valid_word_form("aujourd''hui"))
        self.assertFalse(is_valid_word_form("deux mots"))

    def test_requires_a_string(self):
        with self.assertRaisesRegex(TypeError, "word must be a string"):
            normalize(None)  # type: ignore[arg-type]


if __name__ == "__main__":
    unittest.main()
