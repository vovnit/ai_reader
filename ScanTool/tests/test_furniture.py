import unittest

from furniture import kept_blocks
from ocr_export import Block


def block(kind, content, **box):
    return Block(kind=kind, content=content, page=1, **{"left": 0.1, "top": 0.2, "right": 0.9, "bottom": 0.3, **box})


class FurnitureTests(unittest.TestCase):
    def test_drops_running_heads_and_page_numbers_but_keeps_footnotes(self):
        page = [
            block("header", "A C T I V I T É S"),
            block("text", "Fanny descend du train."),
            block("footer", "1. Un grain de beauté : petite tache sur la peau."),
            block("footer", "12"),
            block("text", "3"),
        ]

        self.assertEqual([b.content for b in kept_blocks(page)], ["Fanny descend du train.", "1. Un grain de beauté : petite tache sur la peau."])

    def test_drops_the_lettering_of_a_full_page_picture(self):
        page = [block("title", "# NINA MAJE", left=0, top=0, right=1, bottom=1)]

        self.assertEqual(kept_blocks(page), [])

    def test_drops_the_facing_pages_picture_in_the_corner(self):
        sliver = block("image", "![img-5.jpeg](img-5.jpeg)", left=0.003, top=0.003, right=0.15, bottom=0.12)
        picture = block("image", "![img-6.jpeg](img-6.jpeg)", left=0.1, top=0.1, right=0.4, bottom=0.25)

        self.assertEqual(kept_blocks([sliver, picture]), [picture])

    def test_drops_the_printed_contents(self):
        lines = ["CHAPITRE 1 Arrivée à Cannes 8", "CHAPITRE 2 Une grande journaliste 20", "Dossiers Cannes 4", "Le cinéma 42", "TEST FINAL 79", "Le texte est enregistré."]
        page = [block("title", "# Sommaire")] + [block("text", line) for line in lines]

        self.assertEqual(kept_blocks(page), [])

    def test_keeps_numbered_exercises(self):
        page = [block("list", "1 Où se situe la ville ?\n2 Quels écrivains ont séjourné à Cannes ?\n3 Qui étaient les premiers habitants ?\n4 Quand ?\n5 Que trouve-t-on ?")]

        self.assertEqual(len(kept_blocks(page)), 1)


if __name__ == "__main__":
    unittest.main()
