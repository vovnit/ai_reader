import json
import os
import re
import sqlite3
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional

from dictionary_data import (
    DEFAULT_WIKTIONARY_PATH,
    DictionaryDataError,
    default_index_path,
    normalize_language_code,
)
from dictionary_sources import validate_definition_source
from normalization import normalize


def structure_definition_entry(record: dict) -> dict:
    definitions = []
    for sense in record.get("senses", []):
        glosses = sense.get("glosses") or []
        if not glosses:
            continue
        definition = {"glosses": glosses}
        if sense.get("tags"):
            definition["tags"] = sense["tags"]
        if sense.get("raw_tags"):
            definition["raw_tags"] = sense["raw_tags"]
        definitions.append(definition)

    return {
        "part_of_speech": record.get("pos"),
        "part_of_speech_title": record.get("pos_title"),
        "etymologies": record.get("etymology_texts") or [],
        "definitions": definitions,
    }


class DefinitionLookup:
    """Indexed lookup of one target language from a Wiktextract JSONL file."""

    def __init__(
        self,
        source_path: Path = DEFAULT_WIKTIONARY_PATH,
        index_path: Optional[Path] = None,
        reporter: Optional[Callable[[str], None]] = None,
        target_language: str = "fr",
        definition_language: str = "fr",
    ) -> None:
        self.definition_language = normalize_language_code(definition_language)
        self.source_path = validate_definition_source(
            source_path,
            self.definition_language,
        )
        self.target_language = normalize_language_code(target_language)
        self.index_path = Path(
            index_path or default_index_path(self.source_path, self.target_language)
        ).resolve()
        self.reporter = reporter or (lambda message: None)
        self.language_pattern = re.compile(
            rb'"lang_code"\s*:\s*"'
            + re.escape(self.target_language.encode("ascii"))
            + rb'"'
        )

    def _source_metadata(self) -> Dict[str, str]:
        if not self.source_path.is_file():
            raise DictionaryDataError(
                "Wiktionary data not found at {}. Run download_dictionaries.py."
                .format(self.source_path)
            )
        stat = self.source_path.stat()
        return {
            "source_path": str(self.source_path),
            "source_size": str(stat.st_size),
            "source_mtime_ns": str(stat.st_mtime_ns),
            "target_language": self.target_language,
            "definition_language": self.definition_language,
        }

    def _is_current(self, metadata: Dict[str, str]) -> bool:
        if not self.index_path.is_file():
            return False
        try:
            with sqlite3.connect(str(self.index_path)) as database:
                stored = dict(database.execute("SELECT key, value FROM metadata"))
                return stored == metadata
        except (sqlite3.DatabaseError, OSError):
            return False

    def ensure_current(self, rebuild: bool = False) -> None:
        metadata = self._source_metadata()
        if not rebuild and self._is_current(metadata):
            return
        self._build(metadata)

    def _build(self, metadata: Dict[str, str]) -> None:
        self.index_path.parent.mkdir(parents=True, exist_ok=True)
        temporary_path = self.index_path.with_name(self.index_path.name + ".tmp")
        try:
            temporary_path.unlink()
        except FileNotFoundError:
            pass

        self.reporter(
            "Building {} definition index from {} (one-time operation)...".format(
                self.target_language, self.source_path
            )
        )
        line_count = 0
        entry_count = 0
        malformed_count = 0
        database = sqlite3.connect(str(temporary_path))
        try:
            database.execute("PRAGMA journal_mode = OFF")
            database.execute("PRAGMA synchronous = OFF")
            database.execute(
                "CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)"
            )
            database.execute(
                "CREATE TABLE entries (word TEXT NOT NULL, offset INTEGER NOT NULL)"
            )

            pending = []
            with self.source_path.open("rb") as source:
                while True:
                    offset = source.tell()
                    line = source.readline()
                    if not line:
                        break
                    line_count += 1
                    if self.language_pattern.search(line[:1024]):
                        try:
                            record = json.loads(line)
                            word = normalize(record["word"]).normalized
                        except (
                            json.JSONDecodeError,
                            KeyError,
                            TypeError,
                            UnicodeDecodeError,
                        ):
                            malformed_count += 1
                            continue
                        if word:
                            pending.append((word, offset))
                            entry_count += 1
                    if len(pending) >= 5000:
                        database.executemany("INSERT INTO entries VALUES (?, ?)", pending)
                        pending.clear()
                    if line_count % 250000 == 0:
                        self.reporter(
                            "  scanned {:,} records ({:.1f} MiB), indexed {:,} {} entries"
                            .format(
                                line_count,
                                source.tell() / (1024 * 1024),
                                entry_count,
                                self.target_language,
                            )
                        )

            if pending:
                database.executemany("INSERT INTO entries VALUES (?, ?)", pending)
            database.execute("CREATE INDEX entries_word ON entries (word)")
            database.executemany(
                "INSERT INTO metadata VALUES (?, ?)", metadata.items()
            )
            database.commit()
        except Exception:
            database.close()
            try:
                temporary_path.unlink()
            except FileNotFoundError:
                pass
            raise
        else:
            database.close()

        os.replace(str(temporary_path), str(self.index_path))
        message = "Definition index ready: {:,} {} entries from {:,} records".format(
            entry_count, self.target_language, line_count
        )
        if malformed_count:
            message += " (skipped {:,} malformed records)".format(malformed_count)
        self.reporter(message + ".")

    def find(self, lemmas: Iterable[str], rebuild: bool = False) -> Dict[str, List[dict]]:
        normalized_lemmas = list(
            dict.fromkeys(normalize(lemma).normalized for lemma in lemmas)
        )
        results = {lemma: [] for lemma in normalized_lemmas}
        if not normalized_lemmas:
            return results

        self.ensure_current(rebuild=rebuild)
        placeholders = ",".join("?" for _ in normalized_lemmas)
        query = "SELECT word, offset FROM entries WHERE word IN ({}) ORDER BY offset".format(
            placeholders
        )
        with sqlite3.connect(str(self.index_path)) as database:
            offsets = list(database.execute(query, normalized_lemmas))

        with self.source_path.open("rb") as source:
            for lemma, offset in offsets:
                source.seek(offset)
                try:
                    record = json.loads(source.readline())
                except (json.JSONDecodeError, UnicodeDecodeError) as error:
                    raise DictionaryDataError(
                        "Indexed Wiktionary record at byte {} is invalid; rebuild the index."
                        .format(offset)
                    ) from error
                if record.get("lang_code") == self.target_language:
                    results[lemma].append(structure_definition_entry(record))
        return results
