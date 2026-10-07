import json
from typing import Dict, List

from book_words import BookWord
from normalization import normalize


class AnswerError(Exception):
    pass


def system(language: str) -> str:
    """`language` is the one the definitions are written in."""
    return """\
Ты составляешь словарик к книге на иностранном языке для читателя, который её читает. \
Тебе дают пронумерованный список слов — в той форме, в какой они стоят в книге, — и под \
каждым один–три отрывка, где слово встречается.

Для каждого слова напиши:
- lemma — начальную (словарную) форму слова, на языке книги;
- form_note — коротко, в какой форме слово стоит в тексте («мн. ч.», «прош. вр., 3 л. ед. ч.»); \
пусто, если это и есть начальная форма;
- meaning — что слово значит в этих отрывках, коротко, как в карманном словаре. Если в отрывках \
оно в разных значениях, перечисли их через «; ». Если слово здесь — часть устойчивого выражения, \
назови выражение и что оно значит.

Слово с апострофом (l'homme, qu'il) объясни целиком: какое слово сокращено и какое стоит после \
апострофа.

Имя или название объясни только по отрывкам — кто или что это, — а не по тому, что ты знаешь \
о книге из других источников. Не рассказывай, что случится дальше.

Ответ — только JSON-объект, без пояснений вокруг; form_note и meaning — на языке «{language}»:
{{"words": [{{"n": номер слова в списке, "lemma": "...", "form_note": "...", "meaning": "..."}}]}}
Объясни каждое слово из списка, ни одного не пропускай.""".format(language=language)


def question(words: List[BookWord]) -> str:
    lines = []
    for number, word in enumerate(words, 1):
        lines.append("{}. {}".format(number, word.spelling))
        lines.extend("   — {}".format(example) for example in word.examples)
    return "\n".join(lines)


def parse_answer(content: str, words: List[BookWord]) -> Dict[str, str]:
    """The definition of each word the answer covers, by form. A word the
    model skipped is simply absent; the next run asks for it again."""
    start, end = content.find("{"), content.rfind("}")
    try:
        items = json.loads(content[start:end + 1])["words"] if 0 <= start < end else None
    except (ValueError, KeyError, TypeError):
        items = None
    if not isinstance(items, list):
        raise AnswerError("the model's answer could not be read: {}".format(content[:200]))

    definitions = {}
    for item in items:
        if not isinstance(item, dict):
            continue
        try:
            number = int(item.get("n"))
        except (TypeError, ValueError):
            continue
        if not 1 <= number <= len(words):
            continue
        form = words[number - 1].form
        definition = _definition(form, item)
        if definition:
            definitions[form] = definition
    return definitions


def line(form: str, definition: str) -> str:
    """One line of the word list: the headword, a tab, the definition."""
    return "{}\t{}\n".format(form, definition)


def _definition(form: str, item: dict) -> str:
    meaning = _clean(item.get("meaning"))
    if not meaning:
        return ""
    lemma = _clean(item.get("lemma"))
    if not lemma or normalize(lemma).normalized == form:
        return meaning
    note = _clean(item.get("form_note"))
    return "{} ({}): {}".format(lemma, note, meaning) if note else "{}: {}".format(lemma, meaning)


def _clean(value) -> str:
    if not isinstance(value, str):
        return ""
    # The apps' word-list readers split on tabs and drop double quotes, which
    # they take for a spreadsheet's quoting.
    return " ".join(value.split()).replace('"', "'")
