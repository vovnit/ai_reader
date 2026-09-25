from typing import Iterable, List

from form_lookup import FormMatch


def extract_lemmas(
    normalized_word: str,
    form_matches: Iterable[FormMatch],
) -> List[str]:
    """Return unique Lexique lemmas, falling back to the normalized word."""
    lemmas = list(dict.fromkeys(match.lemma for match in form_matches))
    return lemmas or [normalized_word]
