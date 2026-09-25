# App Integration Guide

The generated `*.sqlite3` file is a self-contained, read-only dictionary. The
app does not need this Python project or the original Lexique/Wiktionary files
at runtime.

## Minimal Integration

1. Build the language pair and add the resulting file to the app bundle:

   ```bash
   python3 process_dictionaries.py \
     --target-language fr \
     --definition-language en \
     --output dictionary.sqlite3
   ```

   If the app runtime has no zlib decoder, add `--no-compression`. This makes
   the file larger but stores definition payloads as plain JSON.

2. Open the database read-only. If the platform cannot open a bundled asset as
   a normal file (notably Android), copy it once to the app's private files or
   database directory first. Do not copy it into a user-visible directory.

3. At startup, read and validate its metadata:

   ```sql
   SELECT key, value FROM metadata;
   ```

   Require `schema_version = 2`, check that `target_language` and
   `definition_language` are the pair expected by the app, and read
   `payload_encoding`, which is either `json` or `json+zlib`.

4. Normalize user input before querying: convert to Unicode NFC, map typographic
   apostrophes to `'` and Unicode hyphens to `-`, trim and case-fold, then remove
   remaining punctuation or symbols from both ends. Bind the result as a SQL
   parameter rather than constructing SQL from user input.

5. Resolve an inflected form to one or more lemmas:

   ```sql
   SELECT l.id, l.word
   FROM forms AS f
   JOIN lemmas AS l ON l.id = f.lemma_id
   WHERE f.normalized_form = ?
   GROUP BY l.id, l.word
   ORDER BY MIN(f.ordinal);
   ```

   If this returns no rows, use direct-headword fallback. This is also the
   normal path for packs whose target language has no morphology source:

   ```sql
   SELECT id, word FROM lemmas WHERE word = ?;
   ```

6. Fetch definitions for each resolved lemma:

   ```sql
   SELECT payload
   FROM entries
   WHERE lemma_id = ?
   ORDER BY ordinal;
   ```

   For `json`, decode the payload bytes as UTF-8 and parse JSON. For
   `json+zlib`, zlib-decompress the bytes first, then decode UTF-8 and parse
   JSON. Each payload is one structured Wiktionary definition entry ready for
   the app to render.

Keep one read-only connection open or use the platform's normal connection
pool. No schema creation, import, or migration is needed. To ship a dictionary
update, replace the complete file while no connection is using it; do not write
to or migrate the packaged database in place.

## Optional Form Details

To display grammar information alongside a matched form, query `forms` without
grouping and select `f.form`, `f.part_of_speech`, `f.gender`, `f.number`, and
`f.verb_info`. `verb_info` is a JSON array stored as text.

The reference implementation for these operations is
`processed_dictionary.py`.
