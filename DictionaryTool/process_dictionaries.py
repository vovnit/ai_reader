import argparse
import csv
import json
import os
import re
import sqlite3
import sys
import zlib
from pathlib import Path
from typing import Callable, Dict, Optional, Sequence

from definition_lookup import structure_definition_entry
from dictionary_data import (
    DEFAULT_LEXIQUE_PATH,
    DictionaryDataError,
    normalize_language_code,
)
from dictionary_sources import (
    validate_definition_source,
    validate_wikidict_source,
    wiktionary_extract_path,
)
from normalization import normalize


BASE_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT_PATH = BASE_DIR / ".mobile-dictionary.sqlite3"
SCHEMA_VERSION = "2"


class ProcessingError(RuntimeError):
    pass


def _human_size(byte_count: int) -> str:
    size = float(byte_count)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if size < 1024 or unit == "TiB":
            return "{:.1f} {}".format(size, unit)
        size /= 1024
    raise AssertionError("unreachable")


def _source_metadata(prefix: str, path: Path) -> Dict[str, str]:
    stat = path.stat()
    return {
        "{}_path".format(prefix): str(path),
        "{}_size".format(prefix): str(stat.st_size),
        "{}_mtime_ns".format(prefix): str(stat.st_mtime_ns),
    }


def build_processed_dictionary(
    output_path: Path,
    lexique_path: Path = DEFAULT_LEXIQUE_PATH,
    wiktionary_path: Optional[Path] = None,
    target_language: str = "fr",
    definition_language: str = "fr",
    lossless: bool = False,
    compress: bool = True,
    force: bool = False,
    reporter: Callable[[str], None] = print,
    download_missing: bool = True,
    wikidict_path: Optional[Path] = None,
) -> dict:
    """Build a compact SQLite dictionary from Lexique4 and definition data.

    Lossless mode preserves all matching data observable through the current
    lookup JSON contract. French compact mode preserves all forms but limits
    definitions to Lexique-reachable lemmas. Targets without a morphology
    source always retain all direct headwords.
    """
    output_path = Path(output_path).expanduser().resolve()
    lexique_path = Path(lexique_path).expanduser().resolve()
    target_language = normalize_language_code(target_language)
    definition_language = normalize_language_code(definition_language)
    if wiktionary_path is None:
        resolved_source_path = wiktionary_extract_path(
            BASE_DIR,
            definition_language,
        )
        if not resolved_source_path.is_file() and download_missing:
            from download_dictionaries import DownloadError, install_wiktionary

            try:
                install_wiktionary(
                    BASE_DIR,
                    definition_language=definition_language,
                    reporter=reporter,
                )
            except DownloadError as error:
                raise ProcessingError(str(error)) from error
    else:
        resolved_source_path = Path(wiktionary_path).expanduser().resolve()
    wiktionary_path = validate_definition_source(
        resolved_source_path,
        definition_language,
    )
    if wikidict_path is not None:
        wikidict_path = validate_wikidict_source(
            wikidict_path,
            target_language,
            definition_language,
        )
    language_pattern = re.compile(
        rb'"lang_code"\s*:\s*"'
        + re.escape(target_language.encode("ascii"))
        + rb'"'
    )
    source_paths = {lexique_path, wiktionary_path}
    if wikidict_path is not None:
        source_paths.add(wikidict_path)
    if output_path in source_paths:
        raise ProcessingError("Output path must not replace a source dictionary.")
    if output_path.exists() and not force:
        raise ProcessingError(
            "Output already exists: {}. Pass --force to replace it."
            .format(output_path)
        )
    if target_language == "fr" and not lexique_path.is_file():
        raise ProcessingError("Lexique4 data not found: {}".format(lexique_path))
    if not wiktionary_path.is_file():
        raise ProcessingError(
            "Wiktionary JSONL data not found: {}".format(wiktionary_path)
        )
    if wikidict_path is not None and not wikidict_path.is_file():
        raise ProcessingError("Wikidict data not found: {}".format(wikidict_path))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_name(output_path.name + ".tmp")
    try:
        temporary_path.unlink()
    except FileNotFoundError:
        pass

    effective_lossless = lossless or target_language != "fr"
    mode = "lossless" if effective_lossless else "compact"
    encoding = "json+zlib" if compress else "json"
    reporter(
        "Building {} {} -> {} processed dictionary at {}"
        .format(mode, definition_language, target_language, output_path)
    )

    database = sqlite3.connect(str(temporary_path))
    lemma_ids: Dict[str, int] = {}
    form_count = 0
    definition_count = 0
    target_record_count = 0
    malformed_count = 0
    wikidict_entry_count = 0
    malformed_wikidict_count = 0
    line_count = 0
    bytes_scanned = 0
    try:
        database.execute("PRAGMA journal_mode = OFF")
        database.execute("PRAGMA synchronous = OFF")
        database.execute("PRAGMA temp_store = MEMORY")
        database.execute(
            "CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)"
        )
        database.execute(
            "CREATE TABLE lemmas (id INTEGER PRIMARY KEY, word TEXT NOT NULL UNIQUE)"
        )
        database.execute(
            """
            CREATE TABLE forms (
                normalized_form TEXT NOT NULL,
                ordinal INTEGER NOT NULL,
                form TEXT NOT NULL,
                lemma_id INTEGER NOT NULL,
                part_of_speech TEXT NOT NULL,
                gender TEXT,
                number TEXT,
                verb_info TEXT NOT NULL,
                PRIMARY KEY (normalized_form, ordinal)
            ) WITHOUT ROWID
            """
        )
        database.execute(
            """
            CREATE TABLE entries (
                lemma_id INTEGER NOT NULL,
                ordinal INTEGER NOT NULL,
                payload BLOB NOT NULL,
                PRIMARY KEY (lemma_id, ordinal)
            ) WITHOUT ROWID
            """
        )

        if target_language == "fr":
            reporter("Reading French forms from {}...".format(lexique_path))
            form_batch = []
            with lexique_path.open("r", encoding="utf-8-sig", newline="") as source:
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
                    raise ProcessingError(
                        "Lexique4 file has an unsupported header: {}"
                        .format(lexique_path)
                    )
                for ordinal, row in enumerate(rows, start=1):
                    if any(row.get(field) is None for field in required):
                        raise ProcessingError(
                            "Lexique4 has a malformed row at line {}: {}"
                            .format(rows.line_num, lexique_path)
                        )
                    normalized_form = normalize(row["1_Mot"]).normalized
                    lemma = normalize(row["4_Lemme"]).normalized
                    if not normalized_form or not lemma:
                        continue
                    lemma_id = lemma_ids.setdefault(lemma, len(lemma_ids) + 1)
                    verb_info = [
                        value for value in row["9_InfoVER"].split(",") if value
                    ]
                    form_batch.append(
                        (
                            normalized_form,
                            ordinal,
                            row["1_Mot"],
                            lemma_id,
                            row["5_Cgram"],
                            row["7_Genre"] or None,
                            row["8_Nombre"] or None,
                            json.dumps(verb_info, separators=(",", ":")),
                        )
                    )
                    form_count += 1
                    if len(form_batch) >= 5000:
                        database.executemany(
                            "INSERT INTO forms VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                            form_batch,
                        )
                        form_batch.clear()
                if form_batch:
                    database.executemany(
                        "INSERT INTO forms VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                        form_batch,
                    )
            reporter(
                "Loaded {:,} form rows and {:,} Lexique lemmas."
                .format(form_count, len(lemma_ids))
            )
        else:
            reporter(
                "No morphology source configured for {}; retaining direct headwords."
                .format(target_language)
            )
        reporter("Reading definitions from {}...".format(wiktionary_path))
        entry_batch = []
        with wiktionary_path.open("rb") as source:
            for line_count, line in enumerate(source, start=1):
                bytes_scanned += len(line)
                if language_pattern.search(line[:1024]):
                    target_record_count += 1
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
                    else:
                        if word and (effective_lossless or word in lemma_ids):
                            lemma_id = lemma_ids.setdefault(word, len(lemma_ids) + 1)
                            payload = json.dumps(
                                structure_definition_entry(record),
                                ensure_ascii=False,
                                separators=(",", ":"),
                            ).encode("utf-8")
                            if compress:
                                payload = zlib.compress(payload)
                            entry_batch.append((lemma_id, line_count, payload))
                            definition_count += 1
                            if len(entry_batch) >= 5000:
                                database.executemany(
                                    "INSERT INTO entries VALUES (?, ?, ?)", entry_batch
                                )
                                entry_batch.clear()
                if line_count % 250000 == 0:
                    reporter(
                        "  scanned {:,} records ({:.1f} MiB), retained {:,} definitions"
                        .format(
                            line_count,
                            bytes_scanned / (1024 * 1024),
                            definition_count,
                        )
                    )
            if entry_batch:
                database.executemany(
                    "INSERT INTO entries VALUES (?, ?, ?)", entry_batch
                )

        if wikidict_path is not None:
            reporter("Reading Wikidict mappings from {}...".format(wikidict_path))
            entry_batch = []
            with wikidict_path.open("r", encoding="utf-8-sig") as source:
                for wikidict_line_count, line in enumerate(source, start=1):
                    source_title, separator, russian_title = line.rstrip(
                        "\r\n"
                    ).partition("\t")
                    word = normalize(source_title).normalized
                    if not separator or not word or not russian_title:
                        malformed_wikidict_count += 1
                        continue
                    if not effective_lossless and word not in lemma_ids:
                        continue
                    lemma_id = lemma_ids.setdefault(word, len(lemma_ids) + 1)
                    payload = json.dumps(
                        {
                            "part_of_speech": None,
                            "part_of_speech_title": None,
                            "etymologies": [],
                            "definitions": [{"glosses": [russian_title]}],
                        },
                        ensure_ascii=False,
                        separators=(",", ":"),
                    ).encode("utf-8")
                    if compress:
                        payload = zlib.compress(payload)
                    entry_batch.append(
                        (lemma_id, line_count + wikidict_line_count, payload)
                    )
                    definition_count += 1
                    wikidict_entry_count += 1
                    if len(entry_batch) >= 5000:
                        database.executemany(
                            "INSERT INTO entries VALUES (?, ?, ?)", entry_batch
                        )
                        entry_batch.clear()
                if entry_batch:
                    database.executemany(
                        "INSERT INTO entries VALUES (?, ?, ?)", entry_batch
                    )

        database.executemany(
            "INSERT INTO lemmas (id, word) VALUES (?, ?)",
            ((lemma_id, word) for word, lemma_id in lemma_ids.items()),
        )
        metadata = {
            "schema_version": SCHEMA_VERSION,
            "mode": mode,
            "target_language": target_language,
            "definition_language": definition_language,
            "payload_encoding": encoding,
            "form_count": str(form_count),
            "lemma_count": str(len(lemma_ids)),
            "definition_entry_count": str(definition_count),
            "target_record_count": str(target_record_count),
            "malformed_record_count": str(malformed_count),
            "wikidict_entry_count": str(wikidict_entry_count),
            "malformed_wikidict_count": str(malformed_wikidict_count),
        }
        if target_language == "fr":
            metadata.update(_source_metadata("lexique", lexique_path))
        metadata.update(_source_metadata("wiktionary", wiktionary_path))
        if wikidict_path is not None:
            metadata.update(_source_metadata("wikidict", wikidict_path))
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

    os.replace(str(temporary_path), str(output_path))
    size = output_path.stat().st_size
    reporter(
        "Processed dictionary ready: {} with {:,} forms, {:,} lemmas, and {:,} definition entries."
        .format(_human_size(size), form_count, len(lemma_ids), definition_count)
    )
    if malformed_count:
        reporter(
            "Skipped {:,} malformed {} records."
            .format(malformed_count, target_language)
        )
    if malformed_wikidict_count:
        reporter(
            "Skipped {:,} malformed Wikidict rows.".format(
                malformed_wikidict_count
            )
        )
    return {
        "path": str(output_path),
        "mode": mode,
        "target_language": target_language,
        "definition_language": definition_language,
        "size": size,
        "forms": form_count,
        "lemmas": len(lemma_ids),
        "definition_entries": definition_count,
        "scanned_records": line_count,
        "target_records": target_record_count,
        "malformed_records": malformed_count,
        "wikidict_entries": wikidict_entry_count,
        "malformed_wikidict_rows": malformed_wikidict_count,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Build a mobile-friendly SQLite language-pair dictionary."
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--lexique", type=Path, default=DEFAULT_LEXIQUE_PATH)
    parser.add_argument(
        "--wiktionary",
        type=Path,
        help="explicit extract; defaults to <definition-language>-extract.jsonl",
    )
    parser.add_argument(
        "--wikidict",
        type=Path,
        help="optional <target-language>-ru_wiki.txt mappings to merge",
    )
    parser.add_argument(
        "--target-language",
        default="fr",
        help="language code of words to retain (default: fr)",
    )
    parser.add_argument(
        "--definition-language",
        default="fr",
        help="language code of definitions/source edition (default: fr)",
    )
    parser.add_argument(
        "--lossless",
        action="store_true",
        help="retain all matching headwords supported by the current lookup contract",
    )
    parser.add_argument(
        "--no-compression",
        action="store_true",
        help="store definition JSON without zlib compression",
    )
    parser.add_argument("--force", action="store_true")
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        target_language = normalize_language_code(arguments.target_language)
        definition_language = normalize_language_code(arguments.definition_language)
        output_path = arguments.output
        if output_path is None:
            output_path = (
                DEFAULT_OUTPUT_PATH
                if target_language == "fr" and definition_language == "fr"
                else BASE_DIR
                / ".mobile-dictionary-{}-{}.sqlite3".format(
                    definition_language,
                    target_language,
                )
            )
        build_processed_dictionary(
            output_path,
            arguments.lexique,
            arguments.wiktionary,
            target_language=target_language,
            definition_language=definition_language,
            lossless=arguments.lossless,
            compress=not arguments.no_compression,
            force=arguments.force,
            wikidict_path=arguments.wikidict,
        )
    except (
        ProcessingError,
        DictionaryDataError,
        ValueError,
        OSError,
        UnicodeError,
        sqlite3.DatabaseError,
        csv.Error,
    ) as error:
        print("Error: {}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
