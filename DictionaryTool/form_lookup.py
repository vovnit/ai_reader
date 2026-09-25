import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional

from dictionary_data import DEFAULT_LEXIQUE_PATH, DictionaryDataError
from normalization import normalize


@dataclass(frozen=True)
class FormMatch:
    form: str
    lemma: str
    part_of_speech: str
    gender: Optional[str]
    number: Optional[str]
    verb_info: List[str]


def _optional(value: str) -> Optional[str]:
    return value or None


class FormLookup:
    """In-memory index of Lexique4 forms and their lexical metadata."""

    def __init__(self, path: Path = DEFAULT_LEXIQUE_PATH) -> None:
        self.path = Path(path).resolve()
        self._forms: Optional[Dict[str, List[FormMatch]]] = None

    def _load(self) -> None:
        if not self.path.is_file():
            raise DictionaryDataError(
                "Lexique4 data not found at {}. Run download_dictionaries.py."
                .format(self.path)
            )

        forms: Dict[str, List[FormMatch]] = {}
        try:
            with self.path.open("r", encoding="utf-8-sig", newline="") as source:
                rows = csv.DictReader(source, delimiter="\t")
                required = {
                    "1_Mot",
                    "4_Lemme",
                    "5_Cgram",
                    "7_Genre",
                    "8_Nombre",
                    "9_InfoVER",
                }
                if not rows.fieldnames or not required.issubset(rows.fieldnames):
                    raise DictionaryDataError(
                        "Lexique4 file has an unsupported header: {}".format(self.path)
                    )

                for row in rows:
                    if any(row.get(field) is None for field in required):
                        raise DictionaryDataError(
                            "Lexique4 has a malformed row at line {}: {}"
                            .format(rows.line_num, self.path)
                        )
                    normalized_form = normalize(row["1_Mot"]).normalized
                    normalized_lemma = normalize(row["4_Lemme"]).normalized
                    if not normalized_form or not normalized_lemma:
                        continue
                    match = FormMatch(
                        form=row["1_Mot"],
                        lemma=normalized_lemma,
                        part_of_speech=row["5_Cgram"],
                        gender=_optional(row["7_Genre"]),
                        number=_optional(row["8_Nombre"]),
                        verb_info=[
                            value for value in row["9_InfoVER"].split(",") if value
                        ],
                    )
                    forms.setdefault(normalized_form, []).append(match)
        except (UnicodeDecodeError, csv.Error) as error:
            raise DictionaryDataError(
                "Could not parse Lexique4 data at {}: {}".format(self.path, error)
            ) from error

        self._forms = forms

    def find(self, normalized_word: str) -> List[FormMatch]:
        if self._forms is None:
            self._load()
        assert self._forms is not None
        return list(self._forms.get(normalized_word, []))


class DirectFormLookup:
    """Fallback for languages without a configured morphology source."""

    def find(self, normalized_word: str) -> List[FormMatch]:
        del normalized_word
        return []
