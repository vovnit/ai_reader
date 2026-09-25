import re
import unicodedata
from dataclasses import dataclass

APOSTROPHE_TRANSLATION = str.maketrans({
    "’": "'",
    "ʼ": "'",
    "＇": "'",
})

HYPHEN_TRANSLATION = str.maketrans({
    "‐": "-",
    "‑": "-",
    "‒": "-",
    "–": "-",
    "—": "-",
    "−": "-",
})


@dataclass(frozen=True)
class NormalizedWord:
    original: str
    normalized: str
    is_valid: bool


def is_valid_word_form(word: str) -> bool:
    if not word:
        return False

    # Apostrophes and hyphens may separate French word components, but may not
    # appear at an edge or next to another separator.
    parts = re.split(r"[-']", word)
    return all(part and all(character.isalpha() for character in part) for part in parts)


def _is_boundary_punctuation(character: str) -> bool:
    return character.isspace() or unicodedata.category(character)[0] in {"P", "S"}


def normalize(word: str) -> NormalizedWord:
    if not isinstance(word, str):
        raise TypeError("word must be a string")

    normalized = unicodedata.normalize("NFC", word)
    normalized = normalized.translate(APOSTROPHE_TRANSLATION)
    normalized = normalized.translate(HYPHEN_TRANSLATION)
    normalized = normalized.strip().casefold()

    while normalized and _is_boundary_punctuation(normalized[0]):
        normalized = normalized[1:]
    while normalized and _is_boundary_punctuation(normalized[-1]):
        normalized = normalized[:-1]

    return NormalizedWord(
        original=word,
        normalized=normalized,
        is_valid=is_valid_word_form(normalized),
    )
