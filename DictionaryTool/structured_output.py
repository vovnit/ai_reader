from dataclasses import asdict
from typing import Dict, Iterable, List

from form_lookup import FormMatch
from normalization import NormalizedWord


def empty_result(
    normalized_word: NormalizedWord,
    target_language: str,
    definition_language: str,
) -> dict:
    return {
        "query": normalized_word.original,
        "normalized": normalized_word.normalized,
        "target_language": target_language,
        "definition_language": definition_language,
        "valid": normalized_word.is_valid,
        "found": False,
        "lemmas": [],
    }


def build_result(
    normalized_word: NormalizedWord,
    form_matches: Iterable[FormMatch],
    lemmas: Iterable[str],
    definition_entries: Dict[str, List[dict]],
    target_language: str,
    definition_language: str,
) -> dict:
    result = empty_result(
        normalized_word,
        target_language,
        definition_language,
    )
    matches = list(form_matches)
    for lemma in lemmas:
        forms = [asdict(match) for match in matches if match.lemma == lemma]
        result["lemmas"].append(
            {
                "lemma": lemma,
                "forms": forms,
                "entries": definition_entries.get(lemma, []),
            }
        )
    result["found"] = any(
        lemma["forms"] or lemma["entries"] for lemma in result["lemmas"]
    )
    return result
