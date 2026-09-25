import json
import sqlite3
import zlib
from pathlib import Path
from typing import Dict, Iterable, List

from dictionary_data import DictionaryDataError
from form_lookup import FormMatch
from normalization import normalize


SUPPORTED_SCHEMA_VERSIONS = {"1", "2"}


def _metadata(database: sqlite3.Connection, path: Path) -> Dict[str, str]:
    try:
        metadata = dict(database.execute("SELECT key, value FROM metadata"))
    except sqlite3.DatabaseError as error:
        raise DictionaryDataError(
            "Processed dictionary is invalid: {}".format(path)
        ) from error
    if metadata.get("schema_version") not in SUPPORTED_SCHEMA_VERSIONS:
        raise DictionaryDataError(
            "Unsupported processed dictionary schema in {}: {}"
            .format(path, metadata.get("schema_version", "missing"))
        )
    if metadata.get("payload_encoding") not in {"json", "json+zlib"}:
        raise DictionaryDataError(
            "Unsupported definition encoding in {}: {}"
            .format(path, metadata.get("payload_encoding", "missing"))
        )
    return metadata


def processed_dictionary_metadata(path: Path) -> Dict[str, str]:
    path = Path(path).expanduser().resolve()
    if not path.is_file():
        raise DictionaryDataError("Processed dictionary not found: {}".format(path))
    with sqlite3.connect(str(path)) as database:
        return _metadata(database, path)


class ProcessedFormLookup:
    def __init__(self, path: Path) -> None:
        self.path = Path(path).expanduser().resolve()

    def find(self, normalized_word: str) -> List[FormMatch]:
        if not self.path.is_file():
            raise DictionaryDataError(
                "Processed dictionary not found: {}".format(self.path)
            )
        with sqlite3.connect(str(self.path)) as database:
            _metadata(database, self.path)
            rows = database.execute(
                """
                SELECT f.form, l.word, f.part_of_speech, f.gender,
                       f.number, f.verb_info
                FROM forms AS f
                JOIN lemmas AS l ON l.id = f.lemma_id
                WHERE f.normalized_form = ?
                ORDER BY f.ordinal
                """,
                (normalized_word,),
            )
            return [
                FormMatch(
                    form=form,
                    lemma=lemma,
                    part_of_speech=part_of_speech,
                    gender=gender,
                    number=number,
                    verb_info=json.loads(verb_info),
                )
                for form, lemma, part_of_speech, gender, number, verb_info in rows
            ]


class ProcessedDefinitionLookup:
    def __init__(self, path: Path) -> None:
        self.path = Path(path).expanduser().resolve()

    def find(
        self,
        lemmas: Iterable[str],
        rebuild: bool = False,
    ) -> Dict[str, List[dict]]:
        del rebuild
        normalized_lemmas = list(
            dict.fromkeys(normalize(lemma).normalized for lemma in lemmas)
        )
        results = {lemma: [] for lemma in normalized_lemmas}
        if not normalized_lemmas:
            return results
        if not self.path.is_file():
            raise DictionaryDataError(
                "Processed dictionary not found: {}".format(self.path)
            )

        placeholders = ",".join("?" for _ in normalized_lemmas)
        query = """
            SELECT l.word, e.payload
            FROM lemmas AS l
            JOIN entries AS e ON e.lemma_id = l.id
            WHERE l.word IN ({})
            ORDER BY e.ordinal
        """.format(placeholders)
        with sqlite3.connect(str(self.path)) as database:
            metadata = _metadata(database, self.path)
            compressed = metadata["payload_encoding"] == "json+zlib"
            for lemma, payload in database.execute(query, normalized_lemmas):
                if compressed:
                    try:
                        payload = zlib.decompress(payload)
                    except zlib.error as error:
                        raise DictionaryDataError(
                            "Processed definition for {!r} is corrupt: {}"
                            .format(lemma, self.path)
                        ) from error
                try:
                    results[lemma].append(json.loads(payload))
                except (json.JSONDecodeError, UnicodeDecodeError) as error:
                    raise DictionaryDataError(
                        "Processed definition for {!r} is invalid: {}"
                        .format(lemma, self.path)
                    ) from error
        return results
