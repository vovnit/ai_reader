import argparse
import json
import sqlite3
import sys
from pathlib import Path
from typing import Callable, Optional, Sequence

from definition_lookup import DefinitionLookup
from dictionary_data import (
    BASE_DIR,
    DEFAULT_LEXIQUE_PATH,
    DictionaryDataError,
    default_index_path,
    normalize_language_code,
)
from dictionary_sources import (
    validate_definition_source,
    wiktionary_extract_path,
)
from form_lookup import DirectFormLookup, FormLookup
from lemma_extraction import extract_lemmas
from normalization import normalize
from processed_dictionary import (
    ProcessedDefinitionLookup,
    ProcessedFormLookup,
    processed_dictionary_metadata,
)
from structured_output import build_result, empty_result


class DictionaryLookup:
    """Orchestrates the independent dictionary lookup stages."""

    def __init__(
        self,
        lexique_path: Path = DEFAULT_LEXIQUE_PATH,
        wiktionary_path: Optional[Path] = None,
        index_path: Optional[Path] = None,
        reporter: Optional[Callable[[str], None]] = None,
        database_path: Optional[Path] = None,
        target_language: Optional[str] = None,
        definition_language: Optional[str] = None,
        download_missing: bool = True,
    ) -> None:
        if database_path is not None:
            metadata = processed_dictionary_metadata(database_path)
            database_target = metadata.get("target_language", "fr")
            database_definition = metadata.get("definition_language", "fr")
            if (
                target_language is not None
                and normalize_language_code(target_language) != database_target
            ):
                raise DictionaryDataError(
                    "Processed dictionary target language is {}, not {}."
                    .format(database_target, target_language)
                )
            if (
                definition_language is not None
                and normalize_language_code(definition_language) != database_definition
            ):
                raise DictionaryDataError(
                    "Processed dictionary definition language is {}, not {}."
                    .format(database_definition, definition_language)
                )
            self.target_language = database_target
            self.definition_language = database_definition
            self.form_lookup = ProcessedFormLookup(database_path)
            self.definition_lookup = ProcessedDefinitionLookup(database_path)
        else:
            self.target_language = normalize_language_code(target_language or "fr")
            self.definition_language = normalize_language_code(
                definition_language or "fr"
            )
            if wiktionary_path is None:
                resolved_source_path = wiktionary_extract_path(
                    BASE_DIR,
                    self.definition_language,
                )
                if not resolved_source_path.is_file() and download_missing:
                    from download_dictionaries import DownloadError, install_wiktionary

                    try:
                        install_wiktionary(
                            BASE_DIR,
                            definition_language=self.definition_language,
                            reporter=reporter or (lambda message: None),
                        )
                    except DownloadError as error:
                        raise DictionaryDataError(str(error)) from error
            else:
                resolved_source_path = Path(wiktionary_path).expanduser().resolve()
            resolved_source_path = validate_definition_source(
                resolved_source_path,
                self.definition_language,
            )
            resolved_index_path = index_path or default_index_path(
                resolved_source_path,
                self.target_language,
            )
            self.form_lookup = (
                FormLookup(lexique_path)
                if self.target_language == "fr"
                else DirectFormLookup()
            )
            self.definition_lookup = DefinitionLookup(
                resolved_source_path,
                resolved_index_path,
                reporter,
                target_language=self.target_language,
                definition_language=self.definition_language,
            )

    def lookup(self, word: str, rebuild_index: bool = False) -> dict:
        normalized_word = normalize(word)
        if not normalized_word.is_valid:
            return empty_result(
                normalized_word,
                self.target_language,
                self.definition_language,
            )

        form_matches = self.form_lookup.find(normalized_word.normalized)
        lemmas = extract_lemmas(normalized_word.normalized, form_matches)
        definitions = self.definition_lookup.find(
            lemmas, rebuild=rebuild_index
        )
        return build_result(
            normalized_word,
            form_matches,
            lemmas,
            definitions,
            self.target_language,
            self.definition_language,
        )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Look up a word and emit structured dictionary JSON."
    )
    parser.add_argument("word", help="word form to look up")
    parser.add_argument("--lexique", type=Path, default=DEFAULT_LEXIQUE_PATH)
    parser.add_argument(
        "--wiktionary",
        type=Path,
        help="explicit Wiktionary extract; defaults to <definition-language>-extract.jsonl",
    )
    parser.add_argument("--index", type=Path)
    parser.add_argument(
        "--database",
        type=Path,
        help="use a database built by process_dictionaries.py",
    )
    parser.add_argument(
        "--target-language",
        help="language code of the words being defined (default: fr)",
    )
    parser.add_argument(
        "--definition-language",
        help="language code of the definitions/source edition (default: fr)",
    )
    parser.add_argument(
        "--rebuild-index",
        action="store_true",
        help="rebuild the Wiktionary index before looking up the word",
    )
    parser.add_argument("--compact", action="store_true", help="emit JSON on one line")
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        lookup = DictionaryLookup(
            arguments.lexique,
            arguments.wiktionary,
            arguments.index,
            reporter=lambda message: print(message, file=sys.stderr),
            database_path=arguments.database,
            target_language=arguments.target_language,
            definition_language=arguments.definition_language,
        )
        result = lookup.lookup(arguments.word, rebuild_index=arguments.rebuild_index)
    except (
        DictionaryDataError,
        ValueError,
        OSError,
        sqlite3.DatabaseError,
    ) as error:
        print(json.dumps({"error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 2

    indent = None if arguments.compact else 2
    print(json.dumps(result, ensure_ascii=False, indent=indent))
    return 0 if result["found"] else 1


if __name__ == "__main__":
    sys.exit(main())
