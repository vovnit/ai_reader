# Multilingual Dictionary Lookup

This command-line tool selects a target language from a Wiktionary extract and
returns definitions in the language of that Wiktionary edition. French targets
also use Lexique4 for form-to-lemma resolution. It writes stable, structured
JSON to standard output and operational progress to standard error. It also
builds the dictionary packs the apps look words up in.

## Building a pack for the apps

A pack is one SQLite file. The pack of French words with Russian definitions is
built into every app. Three more are for a reader to build here, on a computer,
and add to an app as a file: French-French, English-Russian and
English-English. Building the French-Russian one gives a newer copy of the
built-in pack.

It takes Python 3 and nothing to install. In this folder, each pack is a
download and a build:

```bash
# French-Russian, the one built into the apps, and English-Russian: one download
python3 download_dictionaries.py --definition-language ru
python3 process_dictionaries.py --target-language fr --definition-language ru --output "French-Russian.sqlite3"
python3 process_dictionaries.py --target-language en --definition-language ru --output "English-Russian.sqlite3"

# French-French
python3 download_dictionaries.py --definition-language fr
python3 process_dictionaries.py --target-language fr --definition-language fr --output "French-French.sqlite3"

# English-English
python3 download_dictionaries.py --definition-language en
python3 process_dictionaries.py --target-language en --definition-language en --output "English-English.sqlite3"
```

Always pass `--output`: an app names the pack after its file, and without it
the pack is written as a hidden `.mobile-dictionary….sqlite3`.

What each needs, measured in October 2026 on a four-core Linux machine:

| Pack | Download | Disk at peak | Build | Pack size | Entries |
| --- | --- | --- | --- | --- | --- |
| French-Russian | 295 MiB | 2.8 GiB | 15 s | 16.7 MiB | 23,556 |
| English-Russian | the same | the same | 12 s | 16.8 MiB | 72,228 |
| French-French | 703 MiB | 7.1 GiB | 1 min | 39.2 MiB | 91,309 |
| English-English | 499 MiB | 3.6 GiB | 1.5 min | 298.3 MiB | 1,492,363 |

- **Download.** The first download also fetches Lexique 4 (47 MiB), which only
  the French packs use. Each archive is unpacked next to the scripts, so at its
  peak the disk holds the archive and the unpacked file together; the unpacked
  file stays until you delete it (Russian 2.5 GiB, French 6.4 GiB, English
  3.1 GiB). Here a download and its unpacking took a minute or so; a slower
  connection takes longer. A download cut off part-way resumes where it
  stopped, up to five attempts ten seconds apart.
- **Build.** The times are for a source the disk has just written; read from
  a cold disk, French-Russian took 104 s. The four builds needed under 200 MiB
  of memory.

Then:

- **Check it** before adding it:
  `python3 lookup.py mangent --database "French-French.sqlite3"`.
- **Add it** on any app's Dictionaries screen with Add… (*Add dictionary* on
  iOS, *Add* on the Kindle). The app copies the file in, and the file's name
  becomes the pack's: `French-French.sqlite3` is listed as French-French.
- **Delete the sources** once their packs are built. With little disk, build
  in this order and delete each before the next download:

  ```bash
  rm -rf ru-extract.jsonl ru-extract.jsonl.metadata.json  # after French-Russian and English-Russian
  rm -rf fr-extract.jsonl fr-extract.jsonl.metadata.json  # after French-French
  rm -rf en-extract.jsonl en-extract.jsonl.metadata.json  # after English-English
  rm -rf Lexique4                                         # last: each download fetches it again
  ```

  `lookup.py` without `--database` also leaves an index beside each source,
  a hidden `.…-index.sqlite3`; delete those too.

What the packs hold:

- The French packs carry Lexique's 189,861 written forms, so an inflected word
  finds its lemma: *mangent* finds manger. They keep only the entries of
  Lexique's lemmas; `--lossless` keeps every French headword, which makes
  French-French about 500 MiB.
- The English packs have no table of forms. An inflected word is found only
  where Wiktionary has an entry for it: English-English has *went* (simple past
  of go) and *dogs* (plural of dog), English-Russian has few such entries, and
  *dogs* finds nothing there. English-English keeps every headword, phrases and
  inflected forms among them, which is why it is large.
- Kaikki publishes no extract of the English Wiktionary as it does of the
  other editions. The whole edition is 2.8 GiB packed, mostly other languages'
  words, so the tool downloads only its file of English words. English
  definitions therefore serve English words only: `--definition-language en`
  with any other `--target-language` finds nothing, and writes no pack.

A pack is licensed CC BY-SA 4.0, as its sources are: Wiktionary, by its
contributors, by way of kaikki.org, and for the French packs Lexique 4, by
Boris New, Christophe Pallier and others (lexique.org). When you share a pack,
name them and the license, as the apps' Dictionaries screens do for the
built-in one — for example, "From Lexique 4 (lexique.org) and the French
Wiktionary (fr.wiktionary.org, by way of kaikki.org), licensed CC BY-SA 4.0."

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
missing, lookup downloads the matching compressed Kaikki extract (for English,
Kaikki's file of English words), validates its edition metadata, and installs it
atomically. It uses definitions authored in
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

From the October 2026 Russian edition, the compact French-Russian pack is
16.7 MiB and contains 23,556 definition entries reachable through French
Lexique lemmas.

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

From the October 2026 French edition, compact mode produces a 39.2 MiB
database with 189,861 form rows and 91,309 definition entries. Lossless mode
produces a 502.0 MiB database with all 2,112,351 French definition entries.

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
morphology source is configured. From the October 2026 French edition, the
German-French pack is 139.5 MiB and contains 530,413 definition entries.

## Usage

The prepared dictionary files are used relative to the scripts, not the current
working directory:

```bash
python3 lookup.py "mangent"
```

The first definition lookup builds a language-specific index. This is a one-time
scan of the large JSONL source; subsequent lookups use direct byte offsets.
Rebuild it after manual source changes, or after this tool changes how it
reads a source, with `--rebuild-index`.

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
built from them is too: whoever ships or shares one names its sources and that
license, as the apps' Dictionaries screens do for the built-in pack. Wikidict
is CC0.

## Tests

```bash
python3 -m unittest discover -s tests -t .
```
