import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from book_flow import compose
from ocr_export import Block
from xhtml import chapter_document

LONG = "Elle a de longs cheveux noirs, de grands yeux bleus et surtout le même petit grain de beauté que la star, et c'est"


def block(kind, content, page, folder=Path(".")):
    return Block(kind=kind, content=content, page=page, left=0.1, top=0.2, right=0.9, bottom=0.3, folder=folder)


def texts(chapter):
    return [item.text for item in chapter.items if item.kind == "paragraph"]


class FlowTests(unittest.TestCase):
    def test_joins_a_sentence_cut_by_the_page_and_drops_the_running_head(self):
        pages = [
            [block("title", "# Une star en danger", 1), block("text", "— Reposez-vous et", 1), block("footer", "9", 1)],
            [block("title", "## Une star en danger", 2), block("text", "mangez quelque chose !", 2)],
        ]

        [chapter] = compose(pages)

        self.assertEqual(chapter.title, "Une star en danger")
        self.assertEqual([item.kind for item in chapter.items], ["heading", "paragraph"])
        self.assertEqual(texts(chapter), ["— Reposez-vous et mangez quelque chose !"])

    def test_joins_a_name_after_a_page_break_only_to_a_long_paragraph(self):
        pages = [
            [block("title", "# Livre", 1), block("text", LONG, 1)],
            [block("text", "Nina, enfin.", 2), block("text", "Illustrations de Marie", 2)],
            [block("text", "Une autre page.", 3)],
        ]

        [chapter] = compose(pages)

        self.assertEqual(texts(chapter), [LONG + " Nina, enfin.", "Illustrations de Marie", "Une autre page."])

    def test_keeps_a_picture_inside_the_sentence_it_interrupted_after_it(self):
        with TemporaryDirectory() as folder:
            (Path(folder) / "img-1.jpeg").write_bytes(b"jpeg")
            pages = [[
                block("title", "# Cannes", 1),
                block("text", "Ce n'est pas une nouveauté pour la ville", 1),
                block("image", "![img-1.jpeg](img-1.jpeg)", 1, Path(folder)),
                block("caption", "La Croisette.", 1),
                block("text", "puisque Zola y venait déjà.", 1),
            ]]

            [chapter] = compose(pages)

        self.assertEqual([item.kind for item in chapter.items], ["heading", "paragraph", "image", "caption"])
        self.assertEqual(texts(chapter), ["Ce n'est pas une nouveauté pour la ville puisque Zola y venait déjà."])

    def test_starts_a_chapter_at_its_label_and_drops_its_repeated_heads(self):
        with TemporaryDirectory() as folder:
            (Path(folder) / "img-4.jpeg").write_bytes(b"jpeg")
            pages = [
                [block("title", "# Personnages", 1)],
                [
                    block("image", "![img-4.jpeg](img-4.jpeg)", 2, Path(folder)),
                    block("text", "CHAPITRE 5", 2),
                    block("title", "# Où est Catherine Roman ?", 2),
                    block("text", "Fanny se réveille.", 2),
                ],
                [block("title", "# Où est Catherine Roman ?", 3), block("title", "## CHAPITRE 5", 3), block("text", "Elle part.", 3)],
                [block("title", "# Compréhension écrite", 4), block("text", "Répondez.", 4)],
            ]

            chapters = compose(pages)

        self.assertEqual([chapter.title for chapter in chapters], ["Personnages", "Où est Catherine Roman ?"])
        self.assertEqual([item.kind for item in chapters[0].items], ["heading"])
        self.assertEqual(
            [item.kind for item in chapters[1].items],
            ["image", "label", "heading", "paragraph", "paragraph", "heading", "paragraph"],
        )

    def test_places_a_footnote_after_its_paragraph_with_a_link_the_apps_drop(self):
        pages = [[
            block("title", "# Livre", 1),
            block("text", "Elle a un grain de beauté$^{1}$ sur le lobe¹ de l'oreille.", 1),
            block("text", "Fanny sourit.", 1),
            block("footer", "1. Un grain de beauté : petite tache sur la peau.", 1),
        ]]

        [chapter] = compose(pages)
        document = chapter_document(chapter, chapter.title, "fr", lambda item: "")

        self.assertIn('beauté<a epub:type="noteref" href="#n1-1"><sup>1</sup></a> sur le lobe de', document)
        self.assertIn(
            'oreille.</p>\n<aside id="n1-1" epub:type="footnote"><p>Un grain de beauté : petite tache sur la peau.</p></aside>\n<p>Fanny sourit.',
            document,
        )


if __name__ == "__main__":
    unittest.main()
