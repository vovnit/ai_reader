import io
import json
import tempfile
import threading
import unittest
import zipfile
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

import glossary_prompt
from book_glossary import write_glossary
from book_text import read_epub
from book_words import BookWord, collect_words
from chat_endpoint import MOCK_ENDPOINT, complete

CONTAINER = """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>"""

PACKAGE = """<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>Le Petit Livre</dc:title><dc:creator>Une Autrice</dc:creator>
  </metadata>
  <manifest>
    <item id="one" href="one.xhtml" media-type="application/xhtml+xml"/>
    <item id="two" href="Text/two%20b.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine><itemref idref="two"/><itemref idref="one"/></spine>
</package>"""

CHAPTER = """<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Titre</title>
<style>p {{ color: red }}</style></head><body>{}</body></html>"""


def make_epub(folder: Path) -> Path:
    path = folder / "book.epub"
    with zipfile.ZipFile(str(path), "w") as archive:
        archive.writestr("mimetype", "application/epub+zip")
        archive.writestr("META-INF/container.xml", CONTAINER)
        archive.writestr("OEBPS/content.opf", PACKAGE)
        archive.writestr("OEBPS/one.xhtml", CHAPTER.format("<p>Les maisons étaient vieilles.</p>"))
        archive.writestr(
            "OEBPS/Text/two b.xhtml",
            CHAPTER.format("<h1>Un</h1><p>L’homme est là, c'est-à-dire<br/>ici.</p><script>x</script>"),
        )
    return path


class BookTextTests(unittest.TestCase):
    def test_reads_paragraphs_in_spine_order_without_head_or_scripts(self):
        with tempfile.TemporaryDirectory() as folder:
            book = read_epub(make_epub(Path(folder)))

        self.assertEqual(book.title, "Le Petit Livre")
        self.assertEqual(book.author, "Une Autrice")
        self.assertEqual(
            book.paragraphs,
            ["Un", "L’homme est là, c'est-à-dire", "ici.", "Les maisons étaient vieilles."],
        )


class BookWordsTests(unittest.TestCase):
    def test_forms_follow_the_apps_word_boundaries(self):
        words = collect_words(["L’homme dit : c'est-à-dire 42 fois, l'homme !"])

        self.assertEqual([word.form for word in words], ["l'homme", "dit", "c'est", "à", "dire", "fois"])
        self.assertEqual(words[0].spelling, "L’homme")
        self.assertEqual(len(words[0].examples), 1)

    def test_examples_are_the_first_three(self):
        words = collect_words(["Le chat {}.".format(n) for n in ["un", "deux", "trois", "quatre"]])

        self.assertEqual(words[1].examples, ["Le chat un.", "Le chat deux.", "Le chat trois."])

    def test_an_example_is_a_window_of_its_paragraph(self):
        words = collect_words([" ".join(["mot"] * 10 + ["chat"] + ["mot"] * 10) + "."])

        self.assertEqual(words[1].examples, ["…" + " ".join(["mot"] * 8 + ["chat"] + ["mot"] * 8) + "…"])


class GlossaryPromptTests(unittest.TestCase):
    words = [BookWord("maisons", "maisons"), BookWord("paris", "Paris"), BookWord("est", "est")]

    def test_question_numbers_words_with_their_examples(self):
        words = [BookWord("paris", "Paris", ["à Paris en hiver"])]
        self.assertEqual(glossary_prompt.question(words), "1. Paris\n   — à Paris en hiver")

    def test_answer_becomes_definitions(self):
        answer = """```json
{"words": [
  {"n": 1, "lemma": "maison", "form_note": "мн. ч.", "meaning": "дома"},
  {"n": "2", "lemma": "Paris", "form_note": "", "meaning": "Париж,\\t\\"столица\\""},
  {"n": 9, "lemma": "x", "meaning": "вне списка"},
  {"n": 3, "lemma": "être", "meaning": ""}
]}
```"""
        self.assertEqual(
            glossary_prompt.parse_answer(answer, self.words),
            {"maisons": "maison (мн. ч.): дома", "paris": "Париж, 'столица'"},
        )

    def test_unreadable_answer_is_an_error(self):
        with self.assertRaises(glossary_prompt.AnswerError):
            glossary_prompt.parse_answer("Sorry, I cannot help.", self.words)


class WriteGlossaryTests(unittest.TestCase):
    def test_mock_run_defines_every_word_and_a_rerun_completes_a_partial_file(self):
        with tempfile.TemporaryDirectory() as folder:
            book = read_epub(make_epub(Path(folder)))
            output = Path(folder) / "glossary.tsv"

            missing = write_glossary(book, output, "Russian", MOCK_ENDPOINT, "mock-medium", "", io.StringIO())
            lines = output.read_text(encoding="utf-8").splitlines()
            entries = [line for line in lines if not line.startswith("#")]

            self.assertEqual(missing, 0)
            self.assertTrue(lines[0].startswith("# Glossary of “Le Petit Livre” by Une Autrice"))
            self.assertEqual(len(entries), len(collect_words(book.paragraphs)))
            self.assertIn("maisons\t«maisons» в книге (макет)", entries)

            output.write_text("\n".join(lines[:4]) + "\n", encoding="utf-8")
            write_glossary(book, output, "Russian", MOCK_ENDPOINT, "mock-medium", "", io.StringIO())
            again = output.read_text(encoding="utf-8").splitlines()

            self.assertEqual(sorted(again), sorted(lines))


class EndpointTests(unittest.TestCase):
    def test_asks_again_without_json_mode_when_the_service_refuses_it(self):
        requests = []

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                requests.append((self.path, self.headers["Authorization"], body))
                if "response_format" in body:
                    reply, status = {"message": "unknown field response_format"}, 400
                else:
                    reply, status = {
                        "choices": [{"message": {"content": "{}"}}],
                        "usage": {"prompt_tokens": 12, "completion_tokens": 3},
                    }, 200
                data = json.dumps(reply).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, *arguments):
                pass

        server = HTTPServer(("127.0.0.1", 0), Handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            endpoint = "http://127.0.0.1:{}/v1/".format(server.server_port)
            completion = complete(endpoint, "model", "secret", [{"role": "user", "content": "hi"}])
        finally:
            server.shutdown()
            server.server_close()

        self.assertEqual(completion.content, "{}")
        self.assertEqual((completion.input_tokens, completion.output_tokens), (12, 3))
        self.assertEqual([path for path, _, _ in requests], ["/v1/chat/completions"] * 2)
        self.assertEqual(requests[0][1], "Bearer secret")
        self.assertNotIn("response_format", requests[1][2])


if __name__ == "__main__":
    unittest.main()
