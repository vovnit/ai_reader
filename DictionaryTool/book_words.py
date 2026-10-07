import re
import unicodedata
from dataclasses import dataclass, field
from typing import Dict, Iterable, List

from normalization import normalize

# Letters joined by apostrophes: what the apps' word segmenters (NaturalLanguage,
# Pango, Intl.Segmenter) hand over as one tapped word, so "l'homme" is one form
# and "c'est-à-dire" three.
WORD = re.compile(r"\w+(?:['’ʼ]\w+)*")
EXAMPLES_PER_FORM = 3
# Words of context on each side of an example.
WINDOW = 8


@dataclass
class BookWord:
    # As the apps look it up: normalized the way imported headwords are.
    form: str
    # As the book first writes it, capital and all, which tells a name apart.
    spelling: str
    # Where it first appears; only the start of the book, so a definition of a
    # name cannot give away what happens later.
    examples: List[str] = field(default_factory=list)


def collect_words(paragraphs: Iterable[str]) -> List[BookWord]:
    """Every distinct word form in reading order, with its first examples."""
    words: Dict[str, BookWord] = {}
    for paragraph in paragraphs:
        paragraph = unicodedata.normalize("NFC", paragraph)
        matches = list(WORD.finditer(paragraph))
        for index, match in enumerate(matches):
            normalized = normalize(match.group())
            # Numbers and the like: nothing to define.
            if not normalized.is_valid:
                continue
            word = words.setdefault(normalized.normalized, BookWord(normalized.normalized, match.group()))
            if len(word.examples) < EXAMPLES_PER_FORM:
                example = _window(paragraph, matches, index)
                if example not in word.examples:
                    word.examples.append(example)
    return list(words.values())


def _window(paragraph: str, matches: List[re.Match], index: int) -> str:
    start = 0 if index <= WINDOW else matches[index - WINDOW].start()
    last = index + WINDOW
    end = len(paragraph) if last >= len(matches) - 1 else matches[last].end()
    text = paragraph[start:end]
    return "{}{}{}".format("…" if start else "", text, "…" if end < len(paragraph) else "")
