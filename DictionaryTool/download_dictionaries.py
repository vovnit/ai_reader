import argparse
import gzip
import http.client
import json
import os
import shutil
import stat
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from typing import Callable, Optional, Sequence

from dictionary_data import normalize_language_code
from dictionary_sources import (
    wikidict_pair_path,
    wiktionary_extract_path,
    write_source_metadata,
)


BASE_DIR = Path(__file__).resolve().parent
LEXIQUE_URL = "http://www.lexique.org/databases/Lexique400/Lexique400.zip"
WIKTIONARY_URL_TEMPLATE = (
    "https://kaikki.org/dictionary/downloads/{language}/"
    "{language}-extract.jsonl.gz"
)
# Kaikki publishes no en-extract. The whole English edition is 2.8 GiB packed,
# mostly other languages' words; its file of English words alone is 0.5 GiB
# and is all en-en needs.
WIKTIONARY_URLS = {
    "en": "https://kaikki.org/dictionary/English/kaikki.org-dictionary-English.jsonl.gz",
}
WIKIDICT_RU_URL_TEMPLATE = (
    "https://raw.githubusercontent.com/open-dict-data/wikidict-ru/master/"
    "data/{filename}"
)
LEXIQUE_HEADER = "1_Mot\t2_Phono\t3_Phono_IPA\t4_Lemme"
CHUNK_SIZE = 1024 * 1024
DOWNLOAD_ATTEMPTS = 5
# Seconds between attempts: retried at once, a dropped network fails all of
# them within a second and throws away what was downloaded.
RETRY_PAUSE = 10


class DownloadError(RuntimeError):
    pass


def human_size(byte_count: int) -> str:
    size = float(byte_count)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if size < 1024 or unit == "TiB":
            return "{:.1f} {}".format(size, unit)
        size /= 1024
    raise AssertionError("unreachable")


def download_file(
    url: str,
    destination: Path,
    reporter: Callable[[str], None] = print,
    attempts: int = DOWNLOAD_ATTEMPTS,
) -> None:
    destination = Path(destination)
    temporary_path = destination.with_name(destination.name + ".part")
    try:
        temporary_path.unlink()
    except FileNotFoundError:
        pass

    reporter("Downloading {}".format(url))
    try:
        _download_resuming(url, temporary_path, reporter, attempts)
        if temporary_path.stat().st_size == 0:
            raise DownloadError("Download produced an empty file: {}".format(url))
    except BaseException:
        try:
            temporary_path.unlink()
        except FileNotFoundError:
            pass
        raise
    os.replace(str(temporary_path), str(destination))
    reporter("Downloaded {} to {}".format(human_size(destination.stat().st_size), destination))


def _download_resuming(
    url: str,
    path: Path,
    reporter: Callable[[str], None],
    attempts: int,
) -> None:
    """Fetches url into path. A large download is often cut off short of its
    end, so each further attempt asks only for the rest."""
    total = None
    version = None
    for attempt in range(1, attempts + 1):
        downloaded = path.stat().st_size if path.exists() else 0
        headers = {"User-Agent": "MultilingualDictionaryTool/1.0"}
        if downloaded:
            headers["Range"] = "bytes={}-".format(downloaded)
            if version:
                # A file republished since the first attempt comes whole.
                headers["If-Range"] = version
        try:
            request = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(request, timeout=60) as response:
                # Anything but 206 is the whole file again: the server
                # ignored the range, or the file changed.
                resumed = downloaded > 0 and response.status == 206
                if not resumed:
                    downloaded = 0
                    content_length = response.headers.get("Content-Length")
                    total = int(content_length) if content_length else None
                    version = response.headers.get("ETag") or response.headers.get(
                        "Last-Modified"
                    )
                with path.open("ab" if resumed else "wb") as output:
                    _copy_with_progress(response, output, downloaded, total, reporter)
        except urllib.error.HTTPError as error:
            # A 4xx answer would only be repeated; a 5xx says to try later.
            if error.code < 500:
                raise DownloadError("Failed to download {}: {}".format(url, error)) from error
            problem = str(error)
        except (OSError, http.client.HTTPException) as error:
            problem = str(error) or type(error).__name__
        else:
            if total is None or path.stat().st_size == total:
                return
            problem = "expected {}".format(human_size(total))
        received = human_size(path.stat().st_size if path.exists() else 0)
        if attempt == attempts:
            raise DownloadError(
                "Failed to download {}: {} received in {} attempts ({})"
                .format(url, received, attempts, problem)
            )
        reporter("  stopped at {} ({}); resuming".format(received, problem))
        time.sleep(RETRY_PAUSE)


def _copy_with_progress(
    response,
    output,
    downloaded: int,
    total: Optional[int],
    reporter: Callable[[str], None],
) -> None:
    while True:
        chunk = response.read(CHUNK_SIZE)
        if not chunk:
            return
        output.write(chunk)
        before, downloaded = downloaded, downloaded + len(chunk)
        if total and downloaded * 10 // total > before * 10 // total:
            reporter(
                "  {:3d}% ({}/{})".format(
                    min(100, downloaded * 100 // total),
                    human_size(downloaded),
                    human_size(total),
                )
            )
        elif not total and downloaded // (100 * CHUNK_SIZE) > before // (100 * CHUNK_SIZE):
            reporter("  downloaded {}".format(human_size(downloaded)))


def safe_extract(archive: zipfile.ZipFile, destination: Path) -> None:
    destination = Path(destination).resolve()
    for member in archive.infolist():
        member_path = Path(member.filename)
        mode = member.external_attr >> 16
        if member_path.is_absolute() or ".." in member_path.parts:
            raise DownloadError("Archive contains an unsafe path: {}".format(member.filename))
        if stat.S_ISLNK(mode):
            raise DownloadError("Archive contains an unsupported symlink: {}".format(member.filename))
        target = (destination / member_path).resolve()
        try:
            target.relative_to(destination)
        except ValueError as error:
            raise DownloadError(
                "Archive contains an unsafe path: {}".format(member.filename)
            ) from error
    archive.extractall(str(destination))


def valid_lexique(path: Path) -> bool:
    table = Path(path) / "Lexique4.tsv"
    if not table.is_file():
        return False
    try:
        with table.open("r", encoding="utf-8-sig") as source:
            return source.readline().startswith(LEXIQUE_HEADER)
    except (OSError, UnicodeDecodeError):
        return False


def valid_wiktionary(path: Path) -> bool:
    path = Path(path)
    if not path.is_file() or path.stat().st_size == 0:
        return False
    try:
        with path.open("rb") as source:
            for _ in range(100):
                line = source.readline()
                if not line:
                    return False
                record = json.loads(line)
                if not isinstance(record, dict):
                    return False
                if (
                    isinstance(record.get("word"), str)
                    and isinstance(record.get("lang_code"), str)
                ) or (
                    isinstance(record.get("title"), str)
                    and isinstance(record.get("redirect"), str)
                ):
                    return True
        return False
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, AttributeError):
        return False


def valid_wikidict(path: Path) -> bool:
    path = Path(path)
    if not path.is_file() or path.stat().st_size == 0:
        return False
    try:
        with path.open("r", encoding="utf-8-sig") as source:
            for _ in range(100):
                line = source.readline()
                if not line:
                    return False
                source_title, separator, russian_title = line.rstrip(
                    "\r\n"
                ).partition("\t")
                if separator and source_title and russian_title:
                    return True
        return False
    except (OSError, UnicodeDecodeError):
        return False


def decompress_gzip(
    source_path: Path,
    destination: Path,
    reporter: Callable[[str], None] = print,
) -> None:
    source_path = Path(source_path)
    destination = Path(destination)
    reporter("Decompressing {}...".format(source_path.name))
    written = 0
    next_progress = 500 * CHUNK_SIZE
    try:
        with gzip.open(str(source_path), "rb") as source, destination.open("wb") as output:
            while True:
                chunk = source.read(CHUNK_SIZE)
                if not chunk:
                    break
                output.write(chunk)
                written += len(chunk)
                if written >= next_progress:
                    reporter("  decompressed {}".format(human_size(written)))
                    next_progress += 500 * CHUNK_SIZE
    except (OSError, EOFError) as error:
        try:
            destination.unlink()
        except FileNotFoundError:
            pass
        raise DownloadError("Downloaded Wiktionary archive is invalid") from error
    reporter("Decompressed {}".format(human_size(written)))


def install_lexique(
    data_dir: Path,
    force: bool = False,
    reporter: Callable[[str], None] = print,
) -> None:
    destination = Path(data_dir).resolve() / "Lexique4"
    if valid_lexique(destination) and not force:
        reporter("Lexique4 is already installed at {}; skipping.".format(destination))
        return

    data_dir = Path(data_dir).resolve()
    archive_path = data_dir / ".Lexique4.zip"
    extraction_path = data_dir / ".Lexique4-extract"
    if extraction_path.exists():
        shutil.rmtree(str(extraction_path))
    extraction_path.mkdir(parents=True)
    try:
        download_file(LEXIQUE_URL, archive_path, reporter)
        reporter("Validating and unpacking Lexique4...")
        try:
            with zipfile.ZipFile(str(archive_path)) as archive:
                safe_extract(archive, extraction_path)
        except (OSError, zipfile.BadZipFile) as error:
            raise DownloadError("Lexique4 download is not a valid ZIP archive") from error

        candidates = list(extraction_path.rglob("Lexique4.tsv"))
        valid_candidates = [path.parent for path in candidates if valid_lexique(path.parent)]
        if len(valid_candidates) != 1:
            raise DownloadError(
                "Expected one valid Lexique4.tsv in the archive; found {}"
                .format(len(valid_candidates))
            )
        if destination.exists():
            shutil.rmtree(str(destination))
        shutil.move(str(valid_candidates[0]), str(destination))
        reporter("Installed Lexique4 at {}".format(destination))
    finally:
        try:
            archive_path.unlink()
        except FileNotFoundError:
            pass
        if extraction_path.exists():
            shutil.rmtree(str(extraction_path))


def install_wiktionary(
    data_dir: Path,
    definition_language: str = "fr",
    force: bool = False,
    reporter: Callable[[str], None] = print,
) -> None:
    definition_language = normalize_language_code(definition_language)
    data_dir = Path(data_dir).resolve()
    destination = wiktionary_extract_path(data_dir, definition_language)
    if valid_wiktionary(destination) and not force:
        reporter(
            "{} Wiktionary extract is already installed at {}; skipping."
            .format(definition_language, destination)
        )
        return

    url = WIKTIONARY_URLS.get(definition_language) or WIKTIONARY_URL_TEMPLATE.format(
        language=definition_language
    )
    staging_path = destination.with_name(
        ".{}-extract.jsonl.download".format(definition_language)
    )
    archive_path = destination.with_name(
        ".{}-extract.jsonl.gz.download".format(definition_language)
    )
    try:
        if urllib.parse.urlparse(url).path.endswith(".gz"):
            download_file(url, archive_path, reporter)
            decompress_gzip(archive_path, staging_path, reporter)
        else:
            download_file(url, staging_path, reporter)
        if not valid_wiktionary(staging_path):
            raise DownloadError("Downloaded Wiktionary extract is not valid JSONL")
        os.replace(str(staging_path), str(destination))
        write_source_metadata(destination, definition_language, url)
    finally:
        for temporary_path in (staging_path, archive_path):
            try:
                temporary_path.unlink()
            except FileNotFoundError:
                pass
    reporter(
        "Validated {} Wiktionary extract at {}"
        .format(definition_language, destination)
    )


def install_wikidict_ru(
    data_dir: Path,
    source_language: str,
    force: bool = False,
    reporter: Callable[[str], None] = print,
) -> None:
    source_language = normalize_language_code(source_language)
    destination = wikidict_pair_path(data_dir, source_language)
    if valid_wikidict(destination) and not force:
        reporter(
            "{} -> ru Wikidict data is already installed at {}; skipping."
            .format(source_language, destination)
        )
        return

    destination.parent.mkdir(parents=True, exist_ok=True)
    staging_path = destination.with_name(".{}.download".format(destination.name))
    filename = destination.name
    url = WIKIDICT_RU_URL_TEMPLATE.format(filename=filename)
    try:
        download_file(url, staging_path, reporter)
        if not valid_wikidict(staging_path):
            raise DownloadError(
                "Downloaded Wikidict file is not valid tab-separated data"
            )
        os.replace(str(staging_path), str(destination))
    finally:
        try:
            staging_path.unlink()
        except FileNotFoundError:
            pass
    reporter(
        "Validated {} -> ru Wikidict data at {}"
        .format(source_language, destination)
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Download and validate dictionary lookup data."
    )
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=BASE_DIR,
        help="destination directory (default: directory containing this script)",
    )
    parser.add_argument(
        "--force", action="store_true", help="redownload data that is already present"
    )
    parser.add_argument(
        "--definition-language",
        default="fr",
        help="Wiktionary edition language to download (default: fr)",
    )
    parser.add_argument(
        "--target-language",
        default="fr",
        help="source language for optional Wikidict data (default: fr)",
    )
    parser.add_argument(
        "--wikidict-ru",
        action="store_true",
        help="also download the target-language -> Russian Wikidict pair",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = build_parser().parse_args(argv)
    data_dir = arguments.data_dir.expanduser().resolve()
    print("Dictionary data directory: {}".format(data_dir))
    try:
        data_dir.mkdir(parents=True, exist_ok=True)
        install_lexique(data_dir, force=arguments.force)
        install_wiktionary(
            data_dir,
            definition_language=arguments.definition_language,
            force=arguments.force,
        )
        if arguments.wikidict_ru:
            install_wikidict_ru(
                data_dir,
                source_language=arguments.target_language,
                force=arguments.force,
            )
    except (DownloadError, ValueError, OSError) as error:
        print("Error: {}".format(error), file=sys.stderr)
        return 1
    print("Dictionary data is ready.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
