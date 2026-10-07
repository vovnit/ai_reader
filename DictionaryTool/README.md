# Multilingual Dictionary Lookup

This command-line tool selects a target language from a Wiktionary extract and
returns definitions in the language of that Wiktionary edition. French targets
also use Lexique4 for form-to-lemma resolution. It writes stable, structured
JSON to standard output and operational progress to standard error.

## Pipeline

`lookup.py` orchestrates independent stages so each can be reused or tested in
isolation:

1. `normalization.py` canonicalizes and validates the input word form.
2. `form_lookup.py` finds French Lexique4 rows or uses direct lookup for targets
   without a configured morphology source.
3. `lemma_extraction.py` selects unique lemmas or applies the direct-word fallback.
4. `definition_lookup.py` retrieves the selected `lang_code` from Wiktionary.
5. `structured_output.py` assembles the stable JSON response.

Shared data locations and data errors are defined in `dictionary_data.py`.

## Language Pairs

`--target-language` selects the language of the words. `--definition-language`
selects the Wiktionary edition that supplies the definitions. If its extract is
missing, lookup downloads the matching compressed Kaikki extract, validates its
edition metadata, and installs it atomically. It uses definitions authored in
that edition; it does not machine-translate another edition's definitions.

French words with Russian definitions:

```bash
python3 lookup.py "émouvante" \
  --target-language fr \
  --definition-language ru
```

This resolves the French form through Lexique4, then searches French entries in
Russian Wiktionary. An explicitly supplied mismatched source, such as
`fr-extract.jsonl` with `--definition-language ru`, is rejected.

With the currently downloaded Russian edition, a compact `ru -> fr` processed
pack is 16.6 MiB and contains 23,455 definition entries reachable through
French Lexique lemmas.

The prepared source is from French Wiktionary, so German words with French
definitions can be queried directly:

```bash
python3 lookup.py "Haus" \
  --target-language de \
  --definition-language fr
```

Each target language gets a separate reusable offset index. French continues to
use Lexique4 morphology. Other targets currently support direct headwords and
Wiktionary form-of entries; language-specific morphology remains future work.

## Processed Database

Build a mobile-friendly SQLite database containing every Lexique form and only
definitions reachable through Lexique lemmas:

```bash
python3 process_dictionaries.py
```

Build a lossless database relative to the current lookup JSON contract. This
also retains every French Wiktionary headword used by direct-word fallback:

```bash
python3 process_dictionaries.py --lossless --output french-full.sqlite3
```

Definition payloads are minified JSON compressed independently with zlib. Use
`--no-compression` if the target runtime should read plain JSON, and `--force`
to atomically replace an existing output. The source dictionaries are never
modified.

For the currently prepared data, compact mode produces a 39.2 MiB database
with 189,861 form rows and 91,957 definition entries. Lossless mode produces a
501.4 MiB database with all 2,111,601 French definition entries.

Use either processed database directly:

```bash
python3 lookup.py "mangent" --database .mobile-dictionary.sqlite3
```

Build and use a German-target pack with French definitions:

```bash
python3 process_dictionaries.py \
  --target-language de \
  --definition-language fr \
  --force

python3 lookup.py "Haus" --database .mobile-dictionary-fr-de.sqlite3
```

Non-French targets currently retain all matching headwords because no compact
morphology source is configured. With the prepared data, the `fr -> de` pack is
141.7 MiB and contains 538,753 definition entries.

## Usage

The prepared dictionary files are used relative to the scripts, not the current
working directory:

```bash
python3 lookup.py "mangent"
```

The first definition lookup builds a language-specific index. This is a one-time
scan of the large JSONL source; subsequent lookups use direct byte offsets.
Rebuild it after manual source changes with `--rebuild-index`.

To validate existing data or download missing data:

```bash
python3 download_dictionaries.py
```

Download another definition edition explicitly:

```bash
python3 download_dictionaries.py --definition-language ru
```

The CC0 `open-dict-data/wikidict-ru` data can optionally supplement Russian
definitions with Wikipedia title mappings. These are reference translations,
not dictionary definitions. Download the pair for the selected word language:

```bash
python3 download_dictionaries.py \
  --definition-language ru \
  --target-language fr \
  --wikidict-ru
```

Merge it into the same SQLite payload format when building a pack:

```bash
python3 process_dictionaries.py \
  --target-language fr \
  --definition-language ru \
  --wikidict wikidict-ru/fr-ru_wiki.txt \
  --force
```

Each Wikidict mapping becomes an entry with an empty etymology, no part of
speech, and the Russian Wikipedia title as its single gloss. Existing
Wiktionary entries remain first and the database schema is unchanged.

Use `--data-dir PATH` for another destination or `--force` to replace valid
existing files. Downloads are staged, validated, and atomically installed.

## Book glossaries

`book_glossary.py` writes a glossary of one EPUB: every word form in the book,
each with what it means there, as a tab-separated word list. Added to any of
the apps as a dictionary, it answers lookups in that book without a network —
the apps show the dictionaries' entries whenever the model cannot be reached.
Each app can write the same glossary itself (*Offline glossary* in the reader's
menu), with the same prompt, and continues one added from here: a book's
glossary is the dictionary named `<title> glossary`.

```bash
MISTRAL_API_KEY=... python3 book_glossary.py book.epub
```

It writes `<title> glossary.tsv` here (`--output` for another path), its
definitions in Russian (`--language`), from the apps' default endpoint and
model (`--endpoint`, `--model`; any OpenAI-compatible service). The model is
asked about fifty forms at a time, each with the first three places it appears
— only the first, so a name is never explained by what happens later. Forms
follow the apps' word boundaries, so `l'homme` and `c'est` are words of their
own, and each line reads `form<TAB>lemma (form): meaning`. The Kindle and Qt
apps break a word at an apostrophe — a tap there finds `l` or `homme` — so for
them a glossary written in the app covers those words better.

The glossary is written as answers arrive. Run the same command again after
an interruption, or after a batch the model answered badly, and it asks only
about the words still missing. `--endpoint mock://ai` answers from a stand-in,
for a run that costs nothing.

At the end it reports the tokens used. A short book of 11,000 words has about
2,200 forms, which should come to roughly 170,000 tokens in and 90,000 out; a
novel, six or seven times that.

## Licenses

Lexique 4 and the Wiktionary extracts are licensed CC BY-SA 4.0, and a pack
built from them is too: whatever ships one names its sources and that license,
as the apps' Dictionaries screens do for the bundled pack. Wikidict is CC0.

## Tests

```bash
python3 -m unittest discover -v
```
