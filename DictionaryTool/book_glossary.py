"""Writes a glossary of an EPUB: every word form in the book, each with what it
means there, as a tab-separated word list. Imported into any of the apps as a
dictionary, it answers lookups in that book without a network."""

import argparse
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import List, Optional, Sequence, Set, TextIO

import glossary_prompt
from book_text import BookText, BookTextError, read_epub
from book_words import BookWord, collect_words
from chat_endpoint import EndpointError, complete

# The apps' own defaults, so a glossary reads like their live answers.
DEFAULT_ENDPOINT = "https://api.mistral.ai/v1"
DEFAULT_MODEL = "mistral-medium-3.5"
DEFAULT_LANGUAGE = "Russian"
BATCH_SIZE = 50
PARALLEL_REQUESTS = 4


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("book", type=Path, help="an EPUB")
    parser.add_argument(
        "--output",
        type=Path,
        help="defaults to '<title> glossary.tsv' in the current folder; "
        "an existing glossary is completed, not replaced",
    )
    parser.add_argument("--language", default=DEFAULT_LANGUAGE, help="what the definitions are written in")
    parser.add_argument("--endpoint", default=DEFAULT_ENDPOINT, help="any OpenAI-compatible endpoint, or mock://ai")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    return parser


def default_output(book: BookText) -> Path:
    return Path("{} glossary.tsv".format(re.sub(r'[\\/:*?"<>|]', " ", book.title).strip()))


def defined_forms(output: Path) -> Set[str]:
    """The words an earlier, interrupted or incomplete run already wrote."""
    if not output.exists():
        return set()
    lines = output.read_text(encoding="utf-8").splitlines()
    return {line.split("\t", 1)[0] for line in lines if line and not line.startswith("#")}


def header(book: BookText, language: str, model: str) -> str:
    by = " by {}".format(book.author) if book.author else ""
    return (
        "# Glossary of “{}”{}: every word as the book writes it, and what it means there.\n"
        "# In {}, written by {} with AIReader's DictionaryTool/book_glossary.py.\n"
    ).format(book.title, by, language, model)


def write_glossary(
    book: BookText,
    output: Path,
    language: str,
    endpoint: str,
    model: str,
    token: str,
    progress: TextIO = sys.stderr,
) -> int:
    """Defines the words `output` does not have yet and appends them to it, a
    batch at a time, so an interrupted run keeps what it paid for. Returns how
    many words are still undefined."""
    words = collect_words(book.paragraphs)
    done = defined_forms(output)
    pending = [word for word in words if word.form not in done]
    batches = [pending[i:i + BATCH_SIZE] for i in range(0, len(pending), BATCH_SIZE)]
    print("{}: {} words, {} to define.".format(book.title, len(words), len(pending)), file=progress)

    system = glossary_prompt.system(language)
    input_tokens = output_tokens = defined = 0

    def define(batch: List[BookWord]):
        messages = [
            {"role": "system", "content": system},
            {"role": "user", "content": glossary_prompt.question(batch)},
        ]
        completion = complete(endpoint, model, token, messages)
        return completion, glossary_prompt.parse_answer(completion.content, batch)

    new = not output.exists() or output.stat().st_size == 0
    with output.open("a", encoding="utf-8") as file:
        if new:
            file.write(header(book, language, model))
        pool = ThreadPoolExecutor(PARALLEL_REQUESTS)
        try:
            for future in as_completed([pool.submit(define, batch) for batch in batches]):
                try:
                    completion, definitions = future.result()
                except glossary_prompt.AnswerError as error:
                    print("A batch was skipped: {}".format(error), file=progress)
                    continue
                input_tokens += completion.input_tokens
                output_tokens += completion.output_tokens
                file.writelines(glossary_prompt.line(form, text) for form, text in definitions.items())
                file.flush()
                defined += len(definitions)
                print("Defined {} of {}.".format(defined, len(pending)), file=progress)
        finally:
            pool.shutdown(cancel_futures=True)
            if input_tokens or output_tokens:
                print("Used {} input and {} output tokens.".format(input_tokens, output_tokens), file=progress)

    return len(pending) - defined


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        book = read_epub(arguments.book)
        output = arguments.output or default_output(book)
        missing = write_glossary(
            book,
            output,
            arguments.language,
            arguments.endpoint,
            arguments.model,
            os.environ.get("MISTRAL_API_KEY", ""),
        )
    except (BookTextError, EndpointError, OSError) as error:
        print("Error: {}".format(error), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Stopped; run again to continue.", file=sys.stderr)
        return 1

    print("Wrote {}.".format(output), file=sys.stderr)
    if missing:
        print("{} words are still undefined; run again to fill them in.".format(missing), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
