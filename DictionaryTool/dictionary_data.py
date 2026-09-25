import re
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent
DEFAULT_LEXIQUE_PATH = BASE_DIR / "Lexique4" / "Lexique4.tsv"
DEFAULT_WIKTIONARY_PATH = BASE_DIR / "fr-extract.jsonl"
DEFAULT_INDEX_PATH = BASE_DIR / ".fr-extract-index.sqlite3"


class DictionaryDataError(RuntimeError):
    pass


LANGUAGE_CODE_PATTERN = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")


def normalize_language_code(language_code: str) -> str:
    if not isinstance(language_code, str):
        raise TypeError("language code must be a string")
    normalized = language_code.strip().lower()
    if not LANGUAGE_CODE_PATTERN.fullmatch(normalized):
        raise ValueError("invalid language code: {!r}".format(language_code))
    return normalized


def default_index_path(source_path: Path, target_language: str) -> Path:
    source_path = Path(source_path).expanduser().resolve()
    target_language = normalize_language_code(target_language)
    if source_path == DEFAULT_WIKTIONARY_PATH.resolve() and target_language == "fr":
        return DEFAULT_INDEX_PATH
    return source_path.parent / ".{}-{}-index.sqlite3".format(
        source_path.stem,
        target_language,
    )
