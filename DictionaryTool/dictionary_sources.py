import json
import os
import re
from pathlib import Path
from typing import Optional

from dictionary_data import DictionaryDataError, normalize_language_code


EXTRACT_NAME_PATTERN = re.compile(
    r"^(?P<language>[a-z0-9]+(?:-[a-z0-9]+)*)-extract\.jsonl$"
)
WIKIDICT_NAME_PATTERN = re.compile(
    r"^(?P<source>[a-z0-9]+(?:[_-][a-z0-9]+)*)-ru_wiki\.txt$"
)


def wiktionary_extract_path(data_dir: Path, definition_language: str) -> Path:
    definition_language = normalize_language_code(definition_language)
    return Path(data_dir).expanduser().resolve() / "{}-extract.jsonl".format(
        definition_language
    )


def wikidict_pair_path(data_dir: Path, source_language: str) -> Path:
    source_language = normalize_language_code(source_language)
    filename_language = source_language.replace("-", "_")
    return (
        Path(data_dir).expanduser().resolve()
        / "wikidict-ru"
        / "{}-ru_wiki.txt".format(filename_language)
    )


def source_metadata_path(extract_path: Path) -> Path:
    extract_path = Path(extract_path)
    return extract_path.with_name(extract_path.name + ".metadata.json")


def infer_definition_language(extract_path: Path) -> Optional[str]:
    extract_path = Path(extract_path)
    metadata_path = source_metadata_path(extract_path)
    if metadata_path.is_file():
        try:
            metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
            return normalize_language_code(metadata["definition_language"])
        except (OSError, UnicodeError, json.JSONDecodeError, KeyError, TypeError, ValueError) as error:
            raise DictionaryDataError(
                "Invalid Wiktionary source metadata: {}".format(metadata_path)
            ) from error

    match = EXTRACT_NAME_PATTERN.fullmatch(extract_path.name)
    if match:
        return normalize_language_code(match.group("language"))
    return None


def validate_definition_source(
    extract_path: Path,
    definition_language: str,
) -> Path:
    extract_path = Path(extract_path).expanduser().resolve()
    expected_language = normalize_language_code(definition_language)
    actual_language = infer_definition_language(extract_path)
    if actual_language is None:
        raise DictionaryDataError(
            "Cannot determine the definition language for {}. Name it "
            "<language>-extract.jsonl or provide its metadata sidecar."
            .format(extract_path)
        )
    if actual_language != expected_language:
        raise DictionaryDataError(
            "Wiktionary source {} contains {} definitions, not {} definitions."
            .format(extract_path, actual_language, expected_language)
        )
    return extract_path


def validate_wikidict_source(
    source_path: Path,
    target_language: str,
    definition_language: str,
) -> Path:
    source_path = Path(source_path).expanduser().resolve()
    expected_target = normalize_language_code(target_language)
    expected_definition = normalize_language_code(definition_language)
    match = WIKIDICT_NAME_PATTERN.fullmatch(source_path.name)
    if not match:
        raise DictionaryDataError(
            "Cannot determine the Wikidict language pair for {}. Expected "
            "<source>-ru_wiki.txt.".format(source_path)
        )
    actual_target = normalize_language_code(match.group("source").replace("_", "-"))
    if actual_target != expected_target or expected_definition != "ru":
        raise DictionaryDataError(
            "Wikidict source {} contains {} -> ru mappings, not {} -> {} mappings."
            .format(
                source_path,
                actual_target,
                expected_target,
                expected_definition,
            )
        )
    return source_path


def write_source_metadata(
    extract_path: Path,
    definition_language: str,
    source_url: str,
) -> None:
    extract_path = Path(extract_path).expanduser().resolve()
    metadata_path = source_metadata_path(extract_path)
    temporary_path = metadata_path.with_name(metadata_path.name + ".tmp")
    metadata = {
        "definition_language": normalize_language_code(definition_language),
        "source_url": source_url,
        "format": "wiktextract-jsonl",
    }
    try:
        temporary_path.write_text(
            json.dumps(metadata, ensure_ascii=True, indent=2) + "\n",
            encoding="utf-8",
        )
        os.replace(str(temporary_path), str(metadata_path))
    finally:
        try:
            temporary_path.unlink()
        except FileNotFoundError:
            pass
