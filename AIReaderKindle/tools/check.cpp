// Runs the pure logic from the command line: prompt building, mock replies,
// dictionary formatting, parameter negotiation, EPUB reading, pagination and
// the database. No display needed.
//
//   AIREADER_DATA_DIR=../AIReader/AIReader/Resources ./aireader-check [book.epub [page.png [chapter [page]]]]

#include "../src/Domain/AI/ChatPrompt.hpp"
#include "../src/Domain/AI/DictionaryTool.hpp"
#include "../src/Domain/AI/ExplanationPrompt.hpp"
#include "../src/Domain/AI/MockAI.hpp"
#include "../src/Domain/AI/RequestQuirks.hpp"
#include "../src/Domain/AI/SearchTool.hpp"
#include "../src/Domain/AI/XRayPrompt.hpp"
#include "../src/Domain/AI/WebSearchTool.hpp"
#include "../src/Domain/AI/WordExplanation.hpp"
#include "../src/Domain/Books/BookDocument.hpp"
#include "../src/Domain/Books/EpubNavigation.hpp"
#include "../src/Domain/Books/EpubPackage.hpp"
#include "../src/Domain/Books/HtmlText.hpp"
#include "../src/Domain/Books/LanguageDetector.hpp"
#include "../src/Domain/Cards/AnkiExport.hpp"
#include "../src/Domain/Cards/Card.hpp"
#include "../src/Domain/Cards/MatchRound.hpp"
#include "../src/Domain/Dictionary/DictionaryLookup.hpp"
#include "../src/Domain/Dictionary/WordNormalizer.hpp"
#include "../src/Domain/Formats/DSLDictionaryReader.hpp"
#include "../src/Domain/Formats/DictionaryFormat.hpp"
#include "../src/Support/TextFile.hpp"
#include "../src/Domain/Reading/Illustrations.hpp"
#include "../src/Domain/Reading/Paginator.hpp"
#include "../src/Domain/Reading/WordContext.hpp"
#include "../src/Domain/Books/ReadingPlace.hpp"
#include "../src/Domain/Search/BookSearch.hpp"
#include "Domain/Sync/SyncDocument.hpp"
#include "../src/Domain/Books/BookKey.hpp"
#include "../src/Domain/Books/RemoteBookName.hpp"
#include "../src/Services/BookCorpus.hpp"
#include "../src/Services/ChatApi.hpp"
#include "../src/Services/Database.hpp"
#include "../src/Services/DictionaryDatabase.hpp"
#include "../src/Services/DictionaryPacks.hpp"
#include "../src/Services/Env.hpp"
#include "../src/Services/EpubLoader.hpp"
#include "../src/Services/LibraryStore.hpp"
#include "../src/Services/LibrarySync.hpp"
#include "../src/Services/LookupCache.hpp"
#include "../src/Services/Migrations.hpp"
#include "../src/Services/Paths.hpp"
#include "../src/Services/Sync.hpp"
#include "../src/Services/SyncStore.hpp"
#include "../src/Services/ToolRunner.hpp"
#include "../src/Services/WebDav.hpp"
#include "../src/Services/WordExplainer.hpp"
#include "../src/Support/Files.hpp"
#include "Support/Json.hpp"
#include "../src/Support/Text.hpp"

#include <pango/pangocairo.h>
#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static int failures = 0;

static void check(const char* name, bool ok, const std::string& detail = "") {
    std::printf("%s  %s%s%s\n", ok ? "ok  " : "FAIL", name, detail.empty() ? "" : " — ", detail.c_str());
    if (!ok) ++failures;
}

static void checkJson() {
    auto parsed = Json::parse(R"({"a":[1,2.5,"xé\n"],"b":{"c":true,"d":null},"e":-3})");
    check("json parses", parsed.has_value());
    if (!parsed) return;
    check("json reads nested values", parsed->at("b").at("c").boolean() && parsed->at("a").at(1).number() == 2.5);
    check("json decodes escapes", parsed->at("a").at(2).string() == "xé\n");
    check("json missing keys read as null", parsed->at("nope").isNull() && parsed->at("a").at(9).isNull());
    auto again = Json::parse(parsed->dump());
    check("json round-trips", again && again->dump() == parsed->dump(), parsed->dump());
    check("json rejects garbage", !Json::parse("{\"a\":") && !Json::parse("[1,]"));
}

static void checkEpubPackage() {
    std::string container = R"(<?xml version="1.0"?><container><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>)";
    check("container.xml names the package", EpubPackage::packagePath(container) == "OEBPS/content.opf");

    std::string opf = R"(<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" xmlns:dc="http://purl.org/dc/elements/1.1/">
  <metadata><dc:title>Le Petit Prince</dc:title><dc:creator>Antoine de Saint-Exup&#233;ry</dc:creator><dc:language>fr</dc:language>
    <meta name="cover" content="cover-img"/></metadata>
  <manifest>
    <item id="cover-img" href="images/cover.jpg" media-type="image/jpeg"/>
    <item id="ch1" href="text/ch%201.xhtml" media-type="application/xhtml+xml"/>
    <item id="ch2" href="text/ch2.xhtml" media-type="application/xhtml+xml"/>
    <item id="css" href="style.css" media-type="text/css"/>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx"><itemref idref="ch1"/><itemref idref="css"/><itemref idref="ch2"/></spine>
</package>)";
    EpubPackage package = EpubPackage::parse(opf);
    check("package title, author, language", package.title == "Le Petit Prince" && package.author == "Antoine de Saint-Exupéry" && package.language == "fr",
          package.title + " / " + package.author + " / " + package.language);
    auto order = package.readingOrder();
    check("reading order keeps only markup", order.size() == 2 && order[0].href == "text/ch 1.xhtml" && order[1].id == "ch2");
    check("cover from EPUB 2 meta", package.coverItem() && package.coverItem()->href == "images/cover.jpg");
    check("hrefs resolve against the package folder", EpubPackage::resolve("../images/a.jpg", "OEBPS/text") == "OEBPS/images/a.jpg");
    check("navigation from the EPUB 2 spine", package.navigationItem() && package.navigationItem()->href == "toc.ncx");
    package.items["nav"] = {"nav", "nav.xhtml", "application/xhtml+xml", "scripted nav"};
    check("navigation prefers the EPUB 3 document", package.navigationItem() && package.navigationItem()->id == "nav");
}

static void checkEpubNavigation() {
    std::string ncx = R"(<?xml version="1.0"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/"><navMap>
<navPoint id="n1"><navLabel><text> PREMIER CHAPITRE </text></navLabel><content src="Text/ch1.xhtml"/>
  <navPoint id="n1a"><navLabel><text>Un</text></navLabel><content src="Text/ch1.xhtml#un"/></navPoint>
</navPoint>
<navPoint id="n2"><navLabel><text>CHAPITRE II</text></navLabel><content src="Text/ch2.xhtml"/></navPoint>
<navPoint id="n3"><navLabel><text></text></navLabel><content src="Text/ch3.xhtml"/></navPoint>
</navMap></ncx>)";
    auto entries = EpubNavigation::parse(ncx);
    check("ncx entries, nested and trimmed, blank ones dropped",
          entries.size() == 3 && entries[0].title == "PREMIER CHAPITRE" && entries[0].href == "Text/ch1.xhtml" && entries[0].depth == 0
              && entries[1].title == "Un" && entries[1].href == "Text/ch1.xhtml#un" && entries[1].depth == 1
              && entries[2].title == "CHAPITRE II" && entries[2].depth == 0,
          std::to_string(entries.size()));

    std::string nav = R"(<html xmlns:epub="http://www.idpf.org/2007/ops"><body>
<nav epub:type="landmarks"><ol><li><a href="cover.xhtml">Cover</a></li></ol></nav>
<nav epub:type="toc"><h1>Contents</h1><ol>
<li><a href="ch1.xhtml">Chapitre <span>I</span></a><ol><li><a href="ch1.xhtml#a">Partie A</a></li></ol></li>
<li><span>Sans lien</span></li>
<li><a href="ch2.xhtml">Chapitre II</a></li>
</ol></nav></body></html>)";
    entries = EpubNavigation::parse(nav);
    check("nav toc entries, the landmarks and a linkless heading left out",
          entries.size() == 3 && entries[0].title == "Chapitre I" && entries[0].depth == 0
              && entries[1].title == "Partie A" && entries[1].href == "ch1.xhtml#a" && entries[1].depth == 1
              && entries[2].title == "Chapitre II" && entries[2].depth == 0,
          std::to_string(entries.size()));

    BookDocument document;
    document.contents = {{"One", 0, 0, 0}, {"One b", 0, 40, 1}, {"Two", 2, 0, 0}};
    check("the entry a place falls under",
          document.contentsEntryAt(0, 10) == 0 && document.contentsEntryAt(0, 40) == 1 && document.contentsEntryAt(1, 0) == 1
              && document.contentsEntryAt(2, 5) == 2 && document.contentsEntryAt(9, 0) == 2);
    document.contents.clear();
    check("no entry before the first", document.contentsEntryAt(0, 0) == -1);
}

static void checkHtmlText() {
    std::string markup = R"(<html><head><title>Ignored</title><style>p{}</style></head>
<body><h1>Chapitre  I</h1>
<p>Lorsque j'avais six ans j'ai vu, une fois, une <i>magnifique</i>
image.</p><p>Elle repr&eacute;sentait un serpent boa.<br/>Fin.</p><!-- note --></body></html>)";
    PlainText text = HtmlText::plain(markup);
    check("html becomes paragraphs", text.text == "Chapitre I\nLorsque j'avais six ans j'ai vu, une fois, une magnifique image.\nElle représentait un serpent boa.\nFin.", text.text);
    bool heading = false, italic = false;
    for (const auto& span : text.spans) {
        if (span.kind == TextSpan::Kind::Heading && text.text.substr(span.start, span.end - span.start) == "Chapitre I") heading = true;
        if (span.kind == TextSpan::Kind::Italic && text.text.substr(span.start, span.end - span.start) == "magnifique") italic = true;
    }
    check("html keeps heading and italic spans", heading && italic);

    auto spanned = [](const PlainText& text, TextSpan::Kind kind) {
        std::string found;
        for (const auto& span : text.spans) if (span.kind == kind) found += text.text.substr(span.start, span.end - span.start) + "|";
        return found;
    };
    // The nbsp before the ! must not break, and every Latin-1 name must decode.
    PlainText entities = HtmlText::plain("<p>Un c&oelig;ur &agrave; l&rsquo;&Eacute;cole&nbsp;! &uuml; &#8212; &euro; &shy;a&#173;b</p>");
    check("html decodes named entities, soft hyphens dropped", entities.text == "Un cœur à l’École\xC2\xA0! ü — € ab", entities.text);

    PlainText notes = HtmlText::plain(R"(<body>
<p>Une maison<a href="notes.xhtml#n1" epub:type="noteref"><sup>1</sup></a> vieille, un mot<sup><a href="#fn2">[2]</a></sup> et<a class="c" href="p12.html#n3"><sup>3</sup></a> une note<a href="#fn4">*</a> ici.</p>
<p>Voir <a href="#c3">le chapitre III</a> et le XIX<sup>e</sup> si&egrave;cle, H<sub>2</sub>O.</p>
<aside epub:type="footnote" id="fn2"><p><a href="#r2">2</a> La note deux. <a href="#r2" epub:type="backlink">&#8617;</a></p></aside>
</body>)");
    check("html drops footnote references but keeps a note's own number",
          notes.text == "Une maison vieille, un mot et une note ici.\nVoir le chapitre III et le XIXe siècle, H2O.\n2 La note deux.", notes.text);
    check("html keeps superscripts and subscripts, minus the dropped ones",
          spanned(notes, TextSpan::Kind::Superscript) == "e|" && spanned(notes, TextSpan::Kind::Subscript) == "2|",
          spanned(notes, TextSpan::Kind::Superscript));

    PlainText shape = HtmlText::plain(R"(<body>
<nav epub:type="landmarks" hidden=""><ol><li><a href="cover.xhtml">Cover</a></li></ol></nav>
<nav epub:type="toc"><ol><li><a href="ch1.xhtml">Chapitre I</a></li></ol></nav>
<span epub:type="pagebreak" title="12">12</span><p>Avant.</p><hr/><p>Apr&egrave;s, <cite>Titre</cite> et <q>dit</q>.</p>
<p>Vers un<br/>
  vers deux</p><p>&nbsp;</p><hr/></body>)");
    check("html hides hidden elements and page numbers, blank line at a rule",
          shape.text == "Chapitre I\nAvant.\n\nAprès, Titre et “dit”.\nVers un\nvers deux\n\xC2\xA0", shape.text);
    check("html sets cite in italics", spanned(shape, TextSpan::Kind::Italic) == "Titre|", spanned(shape, TextSpan::Kind::Italic));

    PlainText anchored = HtmlText::plain(R"(<body><p>Avant.</p><h2 id="ch2">Deux</h2><p><a name="old"/>Texte.</p></body>)");
    check("html records where ids and named anchors begin",
          anchored.anchors.count("ch2") && anchored.text.substr(anchored.anchors["ch2"], 4) == "Deux"
              && anchored.anchors.count("old") && anchored.text.substr(anchored.anchors["old"], 5) == "Texte");
}

static void checkLanguage() {
    PlainText french;
    for (int i = 0; i < 12; ++i) {
        french.text += "Lorsque j'avais six ans j'ai vu, une fois, une magnifique image, dans un livre sur la Forêt Vierge. "
                       "Ça représentait un serpent boa qui avalait un fauve. On disait dans le livre que les serpents boas avalent leur proie tout entière, sans la mâcher. ";
    }
    PlainText english;
    for (int i = 0; i < 12; ++i) {
        english.text += "Once when I was six years old I saw a magnificent picture in a book about the primeval forest. "
                        "It was a picture of a boa constrictor in the act of swallowing an animal, and the book said that they swallow their prey whole. ";
    }
    check("detects French prose", LanguageDetector::detect({french}) == "fr", LanguageDetector::detect({french}));
    check("detects English prose", LanguageDetector::detect({english}) == "en", LanguageDetector::detect({english}));
    check("says nothing about a short text", LanguageDetector::detect({PlainText{"Bonjour.", {}, {}, {}}}).empty());
    check("normalizes declared codes", LanguageDetector::code("fr-FR") == "fr" && LanguageDetector::code("EN") == "en" && LanguageDetector::code("fre") == "fr");
}

static DictionaryLookup sampleLookup() {
    DictionaryLookup lookup;
    lookup.query = "maisons";
    lookup.forms.push_back({"maison", "NOM", "f", "p", {}});
    lookup.articles.push_back({"maison", "noun", {"дом", "здание"}, "Bundled"});
    return lookup;
}

static void checkDictionaryLookup() {
    std::string summary = sampleLookup().summary();
    check("lookup summary format", summary ==
          "Dictionary results for “maisons”:\n- form of “maison” (NOM f p)\n- maison [noun]: дом; здание", summary);
    DictionaryLookup empty;
    empty.query = "xyz";
    check("empty lookup summary", empty.summary() == "No dictionary entry for “xyz”.");
    check("normalizer", WordNormalizer::normalize("«L’Été,»") == "l'été" && WordNormalizer::normalize("Paris.") == "paris",
          WordNormalizer::normalize("«L’Été,»"));
}

static void checkMock() {
    std::vector<ChatMessage> messages = {
        ChatMessage::system(ExplanationPrompt::system),
        ChatMessage::user(ExplanationPrompt::question("maisons", "Les maisons étaient vieilles.", sampleLookup())),
    };
    ChatMessage reply = MockAI::reply(messages);
    auto explanation = WordExplanation::decode(reply.content.value_or(""));
    check("mock answers from the article", explanation && explanation->lemma == "maison" && explanation->meaning == "дом" && !explanation->guessed,
          reply.content.value_or("(no content)"));
    check("mock form note mentions grammar", explanation && Text::contains(explanation->formNote, "NOM f p"));

    DictionaryLookup none;
    none.query = "zut";
    std::vector<ChatMessage> empty = {
        ChatMessage::system(ExplanationPrompt::system),
        ChatMessage::user(ExplanationPrompt::question("zut", "Zut alors.", none)),
    };
    ChatMessage call = MockAI::reply(empty);
    check("mock asks the dictionary again when empty", call.toolCalls.size() == 1 && call.toolCalls[0].name == DictionaryTool::toolName);
    empty.push_back(call);
    empty.push_back(ChatMessage::toolResult(none.summary(), call.toolCalls[0].id));
    auto guessed = WordExplanation::decode(MockAI::reply(empty).content.value_or(""));
    check("mock guesses after a second miss", guessed && guessed->guessed && guessed->confidence < 0.5);

    Json body = messages[1].toJson();
    check("messages serialize with content", body.at("role").string() == "user" && body.at("content").isString());
    Json callJson = call.toJson();
    check("tool calls serialize with type", callJson.at("tool_calls").at(0).at("type").string() == "function");
    ChatMessage back = ChatMessage::fromJson(callJson);
    check("tool calls round-trip", back.toolCalls.size() == 1 && back.toolCalls[0].arguments == call.toolCalls[0].arguments);
}

static void checkWebSearch() {
    // Exa's shape, the shape of a bare list, and one wrapped a level deeper
    // all read the same way; an unknown one is handed over as it came.
    Json exa = *Json::parse(R"({"results":[{"title":"Le Grand Meaulnes","url":"https://fr.wikipedia.org/wiki/Le_Grand_Meaulnes","text":"Roman d'Alain-Fournier.\nParu en 1913."},{"name":"Sans adresse","snippet":"Un extrait."}]})");
    auto hits = WebSearchTool::hits(exa);
    check("web hits read title, url and text", hits.size() == 2 && hits[0].title == "Le Grand Meaulnes" && Text::contains(hits[0].url, "wikipedia")
          && hits[0].text == "Roman d'Alain-Fournier. Paru en 1913." && hits[1].title == "Sans adresse" && hits[1].text == "Un extrait.",
          std::to_string(hits.size()));
    check("web hits from a bare list", WebSearchTool::hits(*Json::parse(R"([{"link":"https://a.example","description":"A"}])")).size() == 1);
    check("web hits from a nested list", WebSearchTool::hits(*Json::parse(R"({"data":{"organic":[{"title":"B"}]}})")).size() == 1);
    std::string summary = WebSearchTool::summary("Meaulnes", exa);
    check("web summary numbers the pages", Text::startsWith(summary, "Pages found for “Meaulnes”:\n1. Le Grand Meaulnes — https://")
          && Text::contains(summary, "\n2. Sans adresse\n   Un extrait."), summary);
    check("web summary says when nothing was found", WebSearchTool::summary("x", Json()) == "Nothing was found on the web for “x”."
          && WebSearchTool::summary("x", Json::array()) == "Nothing was found on the web for “x”.");
    check("web summary hands over an unknown shape", Text::contains(WebSearchTool::summary("x", *Json::parse(R"({"answer":"42"})")), "{\"answer\":\"42\"}"));
    std::string longText(2000, 'a');
    Json page = Json::object();
    page.set("title", "T");
    page.set("text", longText);
    auto cut = WebSearchTool::hits(Json(std::vector<Json>{page}));
    check("web hits cut a long text", cut.size() == 1 && cut[0].text.size() < 700 && Text::endsWith(cut[0].text, "…"));

    WebSearchSettings settings = WebSearchSettings::defaults();
    check("web search is off until a key is given", !settings.isConfigured() && settings.provider == "tinyfish");
    settings.apiKey = "k";
    auto request = Json::parse(settings.request("dit \"non\"\nvite", "fr"));
    check("web search request puts the query and language in, escaped", settings.isConfigured() && request
          && request->at("queryParams").at("query").string() == "dit \"non\"\nvite" && request->at("queryParams").at("language").string() == "fr");
    check("web search request falls back to English", Json::parse(settings.request("x", ""))->at("queryParams").at("language").string() == "en");

    // The mock searches the web when asked to, and answers from what came back.
    auto asked = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Поищи Alain-Fournier"}});
    ChatMessage call = MockAI::reply(asked);
    check("mock chat calls search_web", call.toolCalls.size() == 1 && call.toolCalls[0].name == WebSearchTool::toolName
          && WebSearchTool::query(call.toolCalls[0].arguments) == "Alain-Fournier");
    AiSettings mock = AiSettings::defaults();
    mock.endpoint = AiSettings::mockEndpoint;
    std::vector<ChatMessage> conversation = asked;
    ChatMessage answer = ToolRunner::converse(mock, conversation, ChatPrompt::tools(), false, {{}, {}, {}});
    check("tool runner offers and answers the web under the mock", conversation.size() == 4 && conversation[3].role == "tool"
          && Text::startsWith(*conversation[3].content, "Pages found for “Alain-Fournier”")
          && Text::contains(answer.content.value_or(""), "2 страниц"), answer.content.value_or(""));
}

static void checkQuirks() {
    check("quirk: max_tokens", requestQuirkNamed(R"({"error":{"message":"Unsupported parameter","param":"max_tokens"}})") == RequestQuirk::CompletionTokens);
    check("quirk: temperature", requestQuirkNamed(R"({"error":{"message":"x","param":"temperature"}})") == RequestQuirk::DefaultTemperature);
    check("quirk: reasoning", requestQuirkNamed(R"({"error":{"message":"x","param":"reasoning_effort"}})") == RequestQuirk::NoReasoning);
    check("quirk: unrelated", !requestQuirkNamed(R"({"error":{"message":"nope"}})") && !requestQuirkNamed("not json"));
    RequestQuirkStore::shared().learn(RequestQuirk::NoReasoning, "m");
    check("quirk store remembers", RequestQuirkStore::shared().quirks("m").count(RequestQuirk::NoReasoning) == 1);
}

static void checkChatPrompt() {
    auto messages = ChatPrompt::messages(ChatPrompt::pageContext("Page text."), {{true, "Q1"}, {false, "A1"}, {true, "Q2"}});
    check("chat sends the page once", messages.size() == 4 && Text::contains(*messages[1].content, "Page text.") && *messages[3].content == "Q2");
    check("chat keeps the model's turns", messages[2].role == "assistant" && *messages[2].content == "A1");
    WordExplanation explanation{"maison", "мн. ч.", "дом", false, 0.9};
    std::string word = ChatPrompt::wordContext("maisons", "Les maisons.", explanation);
    check("a word seeds a conversation with its explanation", Text::contains(word, "Слово: maisons") && Text::contains(word, "Объяснение: дом"));
    ChatMessage echo = MockAI::reply(ChatPrompt::messages(word, {{true, "Почему?"}}));
    check("mock tells a seeded conversation from a lookup", echo.toolCalls.empty() && Text::contains(echo.content.value_or(""), "«Почему?»"),
          echo.content.value_or("(tool call)"));

    // Asked to find something, the mock searches the book the way a model would.
    auto asked = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Найди Meaulnes"}});
    ChatMessage call = MockAI::reply(asked);
    check("mock chat calls search_book", call.toolCalls.size() == 1 && call.toolCalls[0].name == SearchTool::toolName
          && SearchTool::query(call.toolCalls[0].arguments) == "Meaulnes");
    asked.push_back(call);
    SearchHit hit;
    hit.excerpt = "Meaulnes entra.";
    hit.matchEnd = 8;
    asked.push_back(ChatMessage::toolResult(SearchTool::summary("Meaulnes", {hit, hit}, false), call.toolCalls[0].id));
    check("mock chat counts the passages", Text::contains(MockAI::reply(asked).content.value_or(""), "2 отрывков"));

    // Asked what a word means, the mock opens the dictionary and answers from the article.
    auto meaning = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Что значит maison?"}});
    ChatMessage lookup = MockAI::reply(meaning);
    check("mock chat calls lookup_dictionary", lookup.toolCalls.size() == 1 && lookup.toolCalls[0].name == DictionaryTool::toolName
          && DictionaryTool::word(lookup.toolCalls[0].arguments) == "maison");
    meaning.push_back(lookup);
    meaning.push_back(ChatMessage::toolResult("Dictionary results for “maison”:\n- maison [noun]: дом, здание; семья", lookup.toolCalls[0].id));
    check("mock chat answers from the article", Text::contains(MockAI::reply(meaning).content.value_or(""), "«maison» — дом, здание"));
    check("chat offers the dictionary and the search", ChatPrompt::tools().size() == 2);
}

static void checkBookSearch() {
    std::string text = "Le grand Meaulnes arriva un dimanche. Il pleuvait.\nMeaulnes, lui, ne dit rien! Puis il partit.";
    auto hits = BookSearch::find(text, "meaulnes", 3, 10);
    check("search is case-insensitive and finds every hit", hits.size() == 2 && hits[0].chapter == 3 && hits[1].offset == static_cast<int>(text.find("Meaulnes, lui")));
    check("excerpt is the sentence", hits.size() == 2 && hits[0].excerpt == "Le grand Meaulnes arriva un dimanche." && hits[1].excerpt == "Meaulnes, lui, ne dit rien!",
          hits.empty() ? "" : hits[0].excerpt + " | " + hits[1].excerpt);
    check("excerpt marks the match", hits.size() == 2 && hits[0].excerpt.substr(hits[0].matchStart, hits[0].matchEnd - hits[0].matchStart) == "Meaulnes"
          && hits[1].matchStart == 0);
    check("search respects the limit", BookSearch::find(text, "l", 0, 3).size() == 3);
    check("one hit per sentence", BookSearch::find("Un chat, deux chats. Trois chats.", "chat", 0, 9).size() == 2);
    check("search: accented letters match by case", BookSearch::find("ÉTÉ. Puis été.", "été", 0, 9).size() == 2);
    check("nothing for an empty query", BookSearch::find(text, "", 0, 9).empty() && BookSearch::find(text, "zzz", 0, 9).empty());

    std::string longer(400, 'a');
    longer = "Début " + longer + " milieu cible " + longer + " fin.";
    auto cut = BookSearch::find(longer, "cible", 0, 1);
    check("a long sentence is cut around the match, at word boundaries", cut.size() == 1 && Text::startsWith(cut[0].excerpt, "…")
          && Text::endsWith(cut[0].excerpt, "…") && cut[0].excerpt.size() < 600
          && cut[0].excerpt.substr(cut[0].matchStart, cut[0].matchEnd - cut[0].matchStart) == "cible",
          cut.empty() ? "" : std::to_string(cut[0].excerpt.size()));

    SearchHit hit;
    hit.bookTitle = "Tome 2";
    hit.chapter = 4;
    hit.excerpt = "Il vint.";
    std::string summary = SearchTool::summary("vint", {hit}, true);
    check("search summary names book and chapter", Text::contains(summary, "1. [Tome 2, chapter 5] Il vint."), summary);
    check("empty search summary", Text::contains(SearchTool::summary("x", {}, false), "No passage"));
    check("tools carry the search", ExplanationPrompt::tools().size() == 2 && ExplanationPrompt::tools().at(1).at("function").at("name").string() == SearchTool::toolName);

    // The X-ray asks once more when it has nothing, then reports the count.
    auto xray = XRayPrompt::messages("Meaulnes", {}, false);
    ChatMessage again = MockAI::reply(xray);
    check("mock x-ray searches when given no passages", again.toolCalls.size() == 1 && SearchTool::query(again.toolCalls[0].arguments) == "meaulnes");
    xray.push_back(again);
    xray.push_back(ChatMessage::toolResult(SearchTool::summary("meaulnes", {hit, hit, hit}, false), again.toolCalls[0].id));
    check("mock x-ray answers from the passages", Text::contains(MockAI::reply(xray).content.value_or(""), "3 отрывках"));
}

static void checkWordContext() {
    std::string text = "Les maisons étaient vieilles. Il pleuvait fort.\nNouveau paragraphe ici.";
    WordContext context(text, "fr");
    auto selection = context.selectionAt(static_cast<int>(text.find("aisons")));
    check("tap resolves the word", selection && selection->word == "maisons", selection ? selection->word : "(none)");
    check("tap resolves the sentence", selection && selection->sentence == "Les maisons étaient vieilles.", selection ? selection->sentence : "(none)");
    auto second = context.selectionAt(static_cast<int>(text.find("pleuvait")));
    check("second sentence", second && second->sentence == "Il pleuvait fort.", second ? second->sentence : "(none)");
    auto gap = context.selectionAt(static_cast<int>(text.find(" étaient")));
    check("a tap on a space is not a word", !gap);
    auto paragraph = context.selectionAt(static_cast<int>(text.find("paragraphe")));
    check("paragraph break ends the sentence", paragraph && paragraph->sentence == "Nouveau paragraphe ici.", paragraph ? paragraph->sentence : "(none)");
}

static void checkCards() {
    Lookup lookup;
    lookup.id = 7;
    lookup.word = "a";
    lookup.sentence = "Il a vu. Elle avait a peine parlé, a-t-il dit.";
    lookup.lemma = "avoir";
    lookup.meaning = "иметь";
    Card card = Card::fromLookup(lookup);
    check("card blanks whole words only", card.example == "Il ____ vu. Elle avait ____ peine parlé, ____-t-il dit.", card.example);
    check("card carries the lemma when it differs", card.front == "a" && card.lemma == "avoir" && card.back == "иметь" && card.lookupId == 7);
    lookup.word = lookup.lemma = "été";
    lookup.sentence = "L’été fut chaud.";
    Card same = Card::fromLookup(lookup);
    check("card blanks after an apostrophe and hides a same lemma", same.example == "L’____ fut chaud." && same.lemma.empty(), same.example);

    Lookup plain;
    plain.word = "chat";
    plain.lemma = "chat";
    plain.sentence = "Le chat & le <chien>.";
    plain.meaning = "кот";
    std::string anki = AnkiExport::text({card, Card::fromLookup(plain)}, "AIReader::Le\tLivre");
    check("anki export: headers, a note per line, fields a tab apart, html escaped", anki ==
          "#separator:tab\n#html:true\n#deck:AIReader::Le Livre\n"
          "<b>a</b> (avoir)<br><i>Il a vu. Elle avait a peine parlé, a-t-il dit.</i>\tиметь\n"
          "<b>chat</b><br><i>Le chat &amp; le &lt;chien&gt;.</i>\tкот\n", anki);

    Card fresh, missed, seen, old;
    fresh.lookupId = 1;
    missed.lookupId = 2; missed.wrong = 2; missed.correct = 0; missed.practicedAt = "2026-09-13T00:00:00Z";
    seen.lookupId = 3; seen.correct = 3; seen.practicedAt = "2026-09-12T00:00:00Z";
    old.lookupId = 4; old.correct = 3; old.practicedAt = "2026-09-01T00:00:00Z";
    auto due = Card::due({seen, old, missed, fresh}, 3);
    check("due cards: never practised, then missed, then longest unseen",
          due.size() == 3 && due[0].lookupId == 1 && due[1].lookupId == 2 && due[2].lookupId == 4);

    std::vector<Card> cards;
    for (int i = 0; i < 5; ++i) {
        Card c;
        c.lookupId = i + 1;
        c.front = "w" + std::to_string(i);
        c.back = "m" + std::to_string(i);
        cards.push_back(c);
    }
    bool everShuffled = false, everAligned = false, everBroken = false;
    for (unsigned seed = 1; seed <= 20; ++seed) {
        MatchRound round(cards, seed);
        std::vector<int> sortedFronts = round.fronts(), sortedBacks = round.backs();
        std::sort(sortedFronts.begin(), sortedFronts.end());
        std::sort(sortedBacks.begin(), sortedBacks.end());
        if (sortedFronts != std::vector<int>{0, 1, 2, 3, 4} || sortedBacks != sortedFronts) everBroken = true;
        for (size_t i = 0; i < 5; ++i) if (round.fronts()[i] == round.backs()[i]) everAligned = true;
        if (round.fronts() != std::vector<int>{0, 1, 2, 3, 4}) everShuffled = true;
    }
    check("round shows every card once on each side", !everBroken);
    check("round shuffles and never lines a pair up", everShuffled && !everAligned);

    MatchRound round(cards, 3);
    check("a lone tap resolves nothing", !round.pickFront(2) && round.selectedFront() == 2 && !round.lastPick());
    check("tapping the chosen card lets go of it", !round.pickFront(2) && !round.selectedFront());
    round.pickFront(2);
    auto miss = round.pickBack(4);
    check("a wrong back is a miss", miss && !miss->matched && miss->front == 2 && miss->back == 4 && round.misses() == 1 && !round.isMatched(2));
    check("a miss clears the choice", !round.selectedFront() && !round.selectedBack() && round.lastPick() && !round.lastPick()->matched);
    round.pickBack(2);
    auto hit = round.pickFront(2);
    check("back then front pairs too", hit && hit->matched && round.isMatched(2) && round.matchedCount() == 1);
    check("a paired card takes no more taps", !round.pickFront(2) && !round.selectedFront());
    for (int i : {0, 1, 3, 4}) { round.pickFront(i); round.pickBack(i); }
    check("round completes", round.isComplete() && round.matchedCount() == 5 && round.misses() == 1);
    MatchRound pair({cards[0], cards[1]}, 9);
    check("two cards make a round with swapped rows", pair.fronts()[0] != pair.backs()[0] && pair.fronts()[1] != pair.backs()[1]);
}

static void checkPaginator() {
    PangoFontMap* map = pango_cairo_font_map_get_default();
    PangoContext* context = pango_font_map_create_context(map);
    PlainText chapter;
    for (int i = 0; i < 40; ++i) {
        chapter.text += "Paragraphe " + std::to_string(i) + ": Lorsque j'avais six ans j'ai vu, une fois, une magnifique image, dans un livre sur la Forêt Vierge qui s'appelait « Histoires Vécues ».\n";
    }
    chapter.text.pop_back();
    chapter.spans.push_back({0, 12, TextSpan::Kind::Heading});
    ReadingStyle style;
    PageLayout layout = Paginator::paginate(context, chapter, style, 300, 400, 1.0, "fr");
    bool contiguous = !layout.pages.empty() && layout.pages.front().start == 0
        && layout.pages.back().end == static_cast<int>(chapter.text.size());
    for (size_t i = 1; i < layout.pages.size(); ++i) {
        if (layout.pages[i].start != layout.pages[i - 1].end) contiguous = false;
        if (layout.pages[i].top <= layout.pages[i - 1].top) contiguous = false;
    }
    bool fits = true;
    for (const auto& page : layout.pages) if (page.height > 400 * PANGO_SCALE) fits = false;
    check("pages tile the chapter", contiguous && layout.pages.size() > 3, std::to_string(layout.pages.size()) + " pages");
    // The heading's size must be absolute, like the body's: the Kindle's
    // Pango 1.26 reads a scaled pixel size as points.
    PangoAttrIterator* attributes = pango_attr_list_get_iterator(pango_layout_get_attributes(layout.layout));
    PangoFontDescription* heading = pango_font_description_copy(pango_layout_get_font_description(layout.layout));
    pango_attr_iterator_get_font(attributes, heading, nullptr, nullptr);
    int bodySize = pango_font_description_get_size(pango_layout_get_font_description(layout.layout));
    check("heading is a step larger, as an absolute size",
          pango_font_description_get_size_is_absolute(heading) && pango_font_description_get_size(heading) == static_cast<int>(1.3 * bodySize)
          && pango_font_description_get_weight(heading) == PANGO_WEIGHT_BOLD,
          std::to_string(pango_font_description_get_size(heading)) + " vs body " + std::to_string(bodySize));
    pango_font_description_free(heading);
    pango_attr_iterator_destroy(attributes);
    check("pages fit the height", fits);
    int middle = static_cast<int>(chapter.text.size() / 2);
    int index = layout.pageContaining(middle);
    check("page lookup by offset", layout.pages[index].start <= middle && middle < layout.pages[index].end);
    check("larger type gives more pages", [&] {
        ReadingStyle big = style;
        big.scale = 2.0;
        return Paginator::paginate(context, chapter, big, 300, 400, 1.0, "fr").pages.size() > layout.pages.size();
    }());
    g_object_unref(context);
}

static void checkDatabase(const std::string& folder) {
    Database database(Files::join(folder, "library.sqlite3"));
    check("database opens and migrates", Migrations::migrate(database), database.lastError());
    Env env(database, Files::join(folder, "settings.ini"));

    Book draft;
    draft.title = "Test";
    draft.path = Files::join(folder, "test.epub");
    long long id = env.library.add(draft);
    env.library.savePosition(id, 2, 345, std::nullopt);
    auto stored = env.library.find(id);
    check("library stores books and positions", stored && stored->readingChapter == 2 && stored->readingOffset == 345);

    WebSearchSettings web = WebSearchSettings::defaults();
    web.apiKey = "monid_live_x";
    web.input = R"({"q": "$query"})";
    env.settings.saveWebSearch(web);
    check("settings keep the web search", env.settings.webSearch() == web && env.settings.webSearch().isConfigured());

    LookupContext context{"maisons", "Les maisons.", "fr", id};
    check("cache misses first", !env.lookups.cached(context));
    env.lookups.save(context, {"maison", "pl.", "дом", false, 0.9});
    auto cached = env.lookups.cached(context);
    check("cache hits after save", cached && cached->lemma == "maison");
    env.lookups.save(context, {"maison", "pl.", "здание", false, 0.8});
    check("saving again replaces", env.lookups.all().size() == 1 && env.lookups.all(id)[0].meaning == "здание");

    auto cards = env.cards.all(id);
    check("every lookup is a card", cards.size() == 1 && cards[0].front == "maisons" && cards[0].back == "здание"
          && cards[0].example == "Les ____." && cards[0].practicedAt.empty(), cards.empty() ? "" : cards[0].example);
    env.cards.record(cards[0].lookupId, false);
    env.cards.record(cards[0].lookupId, true);
    cards = env.cards.all();
    check("practice is counted against the card", cards.size() == 1 && cards[0].correct == 1 && cards[0].wrong == 1 && !cards[0].practicedAt.empty());
    check("cards of another book are not listed", env.cards.all(id + 1).empty());
    env.lookups.remove(env.lookups.all()[0].id);
    check("removing a lookup", env.lookups.all().empty());
    {
        Statement orphans(database, "SELECT COUNT(*) FROM cardPractice");
        check("its practice record goes with it", orphans.step() && orphans.integer(0) == 0);
    }

    long long series = env.groups.named("Série");
    check("a group is made once by name", series > 0 && env.groups.named("Série") == series && env.groups.all().size() == 1);
    env.library.assignGroup(id, series);
    check("a book joins a group", env.library.find(id)->groupId == series && env.library.inGroup(series).size() == 1);
    env.groups.remove(series);
    check("a dissolved group leaves its books", env.groups.all().empty() && env.library.find(id)->groupId == 0);

    // A library from before the practice table and the groups gets them on
    // the next start.
    database.exec("DROP TABLE cardPractice");
    database.exec("DROP TABLE lookupTombstones");
    database.exec("DROP TABLE remoteBooks");
    for (const char* column : {"groupID", "placeFraction", "placeSnippet", "placePending", "updatedAt", "remoteName"}) {
        database.exec(std::string("ALTER TABLE books DROP COLUMN ") + column);
    }
    database.exec("DROP TABLE bookGroups");
    database.setUserVersion(1);
    check("an older library migrates forward", Migrations::migrate(database) && database.userVersion() == 5
          && Statement(database, "SELECT lookupID FROM cardPractice").isValid()
          && Statement(database, "SELECT id FROM bookGroups").isValid()
          && Statement(database, "SELECT placeSnippet, updatedAt FROM books").isValid()
          && Statement(database, "SELECT word FROM lookupTombstones").isValid()
          && Statement(database, "SELECT remoteName FROM books").isValid()
          && Statement(database, "SELECT name FROM remoteBooks").isValid(), database.lastError());

    auto packs = env.packs.all();
    check("bundled pack is listed", packs.size() == 1 && packs[0].isBundled() && packs[0].isEnabled);
    env.packs.setEnabled(packs[0].id, false);
    check("packs can be disabled", env.packs.enabled().empty());
    env.packs.setEnabled(packs[0].id, true);

    AiSettings ai{"mock://ai", "t1", "t2", "mock-medium"};
    env.settings.saveAi(ai);
    check("settings round-trip", env.settings.ai() == ai);
    check("token follows the endpoint", ai.token() == "t1" && AiSettings{"https://api.openai.com/v1", "t1", "t2", "gpt"}.token() == "t2");
    g_unsetenv("MISTRAL_API_KEY");
    check("no token ships with the app", AiSettings::defaults().apiKey.empty());
    ReadingStyle style;
    style.scale = 1.7;
    style.fontName = "Georgia";
    env.settings.saveStyle(style);
    check("style round-trip", env.settings.style() == style);
    env.settings.overrideForRun("https://example/v1", "");
    check("run overrides win without saving", env.settings.ai().endpoint == "https://example/v1" && env.settings.ai().model == "mock-medium");
}

static void checkReadingPlace() {
    std::string chapter = "Première phrase du chapitre. Deuxième phrase, un peu plus longue, qui continue. Troisième.";
    int offset = static_cast<int>(chapter.find("Deuxième"));
    ReadingPlace place = ReadingPlace::at(3, chapter, offset);
    check("place keeps whole words", Text::startsWith(place.snippet, "Deuxième phrase") && place.snippet.back() != ' ' && place.chapter == 3, place.snippet);
    check("place resolves by its words", place.resolve(chapter) == offset);
    ReadingPlace half;
    half.fraction = 0.5;
    half.snippet = "absent";
    check("place falls back to the fraction", half.resolve("0123456789") == 5);
    check("place survives a re-rendering", place.resolve("Prologue ajouté. " + chapter) == offset + static_cast<int>(std::string("Prologue ajouté. ").size()));
    check("a fraction never lands inside a character", ReadingPlace{0, 0.5, ""}.resolve("ééé") % 2 == 0);
    // As the iOS app renders it: a picture, non-breaking spaces, a paragraph break.
    std::string ios = "￼\nAlors j’ai dessiné.\nIl regarda, puis : rien.";
    std::string kindle = "Alors j’ai dessiné. Il regarda, puis : rien.\n￼\n";
    ReadingPlace from = ReadingPlace::at(0, ios, 0);
    check("a snippet ignores pictures and kinds of space", from.snippet == "Alors j’ai dessiné. Il regarda, puis : rien." && from.resolve(kindle) == 0, from.snippet);
    std::string repeated = "Oui. Non. Oui. Non. Oui. Non.";
    check("the occurrence nearest the fraction is taken", ReadingPlace{0, 0.5, "Oui."}.resolve(repeated) == 10 && ReadingPlace{0, 0.95, "Oui."}.resolve(repeated) == 20);
}

static SyncDocument::BookRecord bookRecord(const std::string& key, const std::string& at, const std::string& group = "", int chapter = 1) {
    SyncDocument::BookRecord record;
    record.key = key;
    record.title = key;
    record.language = "fr";
    record.group = group;
    record.chapter = chapter;
    record.fraction = 0.5;
    record.snippet = "x";
    record.updatedAt = at;
    return record;
}

static SyncDocument::LookupRecord lookupRecord(const std::string& word, const std::string& at, const std::string& book = "", bool deleted = false, int correct = 0) {
    SyncDocument::LookupRecord record;
    record.word = word;
    record.sentence = "s";
    record.lemma = word;
    record.meaning = "m";
    record.book = book;
    record.lookedUpAt = at;
    record.correct = correct;
    record.updatedAt = at;
    record.deleted = deleted;
    return record;
}

static void checkSyncDocument() {
    // As the iOS app writes it: absent values as null.
    auto parsed = SyncDocument::parse(R"({"version":1,"books":[{"author":null,"chapter":2,"fraction":0.25,"group":"Série","key":"a|","language":"fr","snippet":"Il vint","title":"A","updatedAt":"2026-09-16T10:00:00Z"}],
        "lookups":[{"book":null,"confidence":0.9,"correct":1,"deleted":false,"formNote":"","guessed":false,"language":"fr","lemma":"un","lookedUpAt":"2026-09-16T09:00:00Z","meaning":"один","practicedAt":"2026-09-16T10:00:00Z","sentence":"s","updatedAt":"2026-09-16T10:00:00Z","word":"un","wrong":0}]})");
    check("sync document parses", parsed && parsed->books.size() == 1 && parsed->lookups.size() == 1);
    if (!parsed) return;
    check("sync document reads a place and a group", parsed->books[0].chapter == 2 && parsed->books[0].fraction == 0.25 && parsed->books[0].snippet == "Il vint" && parsed->books[0].group == "Série");
    check("sync document reads practice", parsed->lookups[0].correct == 1 && parsed->lookups[0].practicedAt == "2026-09-16T10:00:00Z" && parsed->lookups[0].book.empty());
    auto again = SyncDocument::parse(parsed->dump());
    check("sync document round-trips", again && *again == *parsed, parsed->dump());
    check("absent values are written as null", Text::contains(parsed->dump(), "\"author\":null") && Text::contains(parsed->dump(), "\"book\":null"));

    SyncDocument local;
    local.books = {bookRecord("a|", "2026-09-16T10:00:00Z", "", 2), bookRecord("b|", "2026-09-16T09:00:00Z", "Série")};
    local.lookups = {lookupRecord("un", "2026-09-16T10:00:00Z", "", false, 3), lookupRecord("deux", "2026-09-16T08:00:00Z"), lookupRecord("trois", "2026-09-16T11:00:00Z", "", true)};
    SyncDocument remote;
    remote.books = {bookRecord("a|", "2026-09-16T09:00:00Z", "", 1), bookRecord("c|", "2026-09-16T09:00:00Z")};
    remote.lookups = {lookupRecord("un", "2026-09-16T09:00:00Z", "b|"), lookupRecord("deux", "2026-09-16T09:00:00Z", "", true), lookupRecord("trois", "2026-09-16T12:00:00Z"), lookupRecord("quatre", "2026-09-16T09:00:00Z")};
    SyncDocument merged = SyncDocument::merge(local, remote);
    std::map<std::string, SyncDocument::BookRecord> books;
    for (const auto& record : merged.books) books[record.key] = record;
    std::map<std::string, SyncDocument::LookupRecord> words;
    for (const auto& record : merged.lookups) words[record.word] = record;
    check("newer book record wins", books["a|"].chapter == 2 && books.size() == 3);
    check("newer lookup wins and keeps the other side's book", words["un"].correct == 3 && words["un"].book == "b|");
    check("a newer tombstone deletes", words["deux"].deleted);
    check("a lookup made again after deletion comes back", !words["trois"].deleted);
    check("unknown records are kept", words.count("quatre") == 1 && merged.lookups.size() == 4);
    check("merge is deterministic", SyncDocument::merge(remote, local) == merged);
    check("book key normalizes", BookKey::make("  Le  Grand\tMeaulnes ", "Alain-Fournier") == "le grand meaulnes|alain-fournier");
}

static void checkSyncStore(const std::string& folder) {
    Database database(Files::join(folder, "sync.sqlite3"));
    Migrations::migrate(database);
    Env env(database, Files::join(folder, "sync-settings.ini"));

    Book draft;
    draft.title = "Le Grand Meaulnes";
    draft.author = "Alain-Fournier";
    draft.path = Files::join(folder, "meaulnes.epub");
    long long id = env.library.add(draft);
    check("a book never read here has no date, so any other device's record wins",
          SyncStore::exportAll(env).books[0].updatedAt.empty()
          && SyncDocument::merge(SyncStore::exportAll(env), SyncDocument{{bookRecord("le grand meaulnes|alain-fournier", "2000-01-01T00:00:00Z")}, {}}).books[0].updatedAt == "2000-01-01T00:00:00Z");
    env.library.savePosition(id, 1, 10, ReadingPlace{1, 0.1, "Il vint"});
    env.lookups.save({"maisons", "Les maisons.", "fr", id}, {"maison", "pl.", "дом", false, 0.9});
    long long lookupId = env.lookups.all()[0].id;
    env.cards.record(lookupId, true);

    SyncDocument exported = SyncStore::exportAll(env);
    check("export names the book by title and author", exported.books.size() == 1 && exported.books[0].key == "le grand meaulnes|alain-fournier"
          && exported.books[0].chapter == 1 && exported.books[0].snippet == "Il vint" && !exported.books[0].updatedAt.empty());
    check("export carries the lookup with its practice and book", exported.lookups.size() == 1 && exported.lookups[0].book == exported.books[0].key
          && exported.lookups[0].correct == 1 && exported.lookups[0].updatedAt == exported.lookups[0].practicedAt);

    // The other device read further, put the book in a series, learned a
    // word, practised this one more, and deleted another.
    SyncDocument remote;
    SyncDocument::BookRecord book = exported.books[0];
    book.group = "Série";
    book.chapter = 3;
    book.fraction = 0.4;
    book.snippet = "Plus loin";
    book.updatedAt = "2999-01-01T00:00:00Z";
    remote.books = {book};
    SyncDocument::LookupRecord practised = exported.lookups[0];
    practised.correct = 5;
    practised.practicedAt = practised.updatedAt = "2999-01-01T00:00:00Z";
    SyncDocument::LookupRecord fresh = lookupRecord("chat", "2999-01-01T00:00:00Z", book.key);
    fresh.sentence = "Le chat.";
    SyncDocument::LookupRecord gone = lookupRecord("chien", "2999-01-01T00:00:00Z", "", true);
    remote.lookups = {practised, fresh, gone};

    SyncDocument merged = SyncDocument::merge(exported, remote);
    SyncStore::Applied applied = SyncStore::apply(env, merged);
    auto stored = env.library.find(id);
    check("applying moves the book into the group", applied.books == 1 && stored && stored->groupId && env.groups.find(stored->groupId)->name == "Série");
    check("applying leaves the place pending for the reader", stored && stored->placePending && stored->readingChapter == 3 && stored->place && stored->place->snippet == "Plus loin");
    auto cards = env.cards.all();
    check("applying writes the practice and the new word", applied.lookups == 3 && cards.size() == 2 && cards[0].front == "chat" && cards[1].correct == 5, std::to_string(cards.size()));
    check("applying records the deletion", env.lookups.tombstones().size() == 1 && env.lookups.tombstones()[0].word == "chien");
    check("the new word belongs to the book here", env.lookups.all(id).size() == 2);
    check("a second sync has nothing to apply", SyncStore::exportAll(env) == merged && SyncStore::apply(env, merged).lookups == 0);

    env.lookups.remove(env.lookups.all(id)[0].id);
    check("a deletion here becomes a tombstone", env.lookups.tombstones().size() == 2);
    SyncDocument after = SyncStore::exportAll(env);
    int deleted = 0;
    for (const auto& record : after.lookups) deleted += record.deleted ? 1 : 0;
    check("tombstones are exported", deleted == 2 && after.lookups.size() == 3);
}

/// Against a real server, when `AIREADER_SYNC_URL` (and `_USER`, `_PASSWORD`)
/// name one: a round trip through the folder, and the iOS app's file if it
/// has left one there.
static void checkSyncServer(const std::string& folder) {
    const char* url = g_getenv("AIREADER_SYNC_URL");
    if (!url) return;
    SyncSettings settings{url, g_getenv("AIREADER_SYNC_USER") ? g_getenv("AIREADER_SYNC_USER") : "",
                          g_getenv("AIREADER_SYNC_PASSWORD") ? g_getenv("AIREADER_SYNC_PASSWORD") : ""};
    Database database(Files::join(folder, "server-sync.sqlite3"));
    Migrations::migrate(database);
    Env env(database, Files::join(folder, "server-sync.ini"));
    Book draft;
    draft.title = "Le Grand Meaulnes";
    draft.author = "Alain-Fournier";
    draft.path = Files::join(folder, "meaulnes.epub");
    long long id = env.library.add(draft);
    env.library.savePosition(id, 1, 10, ReadingPlace{1, 0.1, "Il vint"});
    env.lookups.save({"maisons", "Les maisons.", "fr", id}, {"maison", "pl.", "дом", false, 0.9});
    try {
        SyncDocument remote = Sync::fetch(settings);
        SyncDocument merged;
        Sync::Report report = Sync::reconcile(env, remote, merged);
        if (report.uploaded) Sync::store(settings, merged);
        SyncDocument again = Sync::fetch(settings);
        check("sync server round trip", again == merged, report.summary());
        SyncDocument second;
        check("second sync has nothing to send", !Sync::reconcile(env, again, second).uploaded);
        std::printf("      %s\n", report.summary().c_str());
    } catch (const std::exception& failure) {
        check("sync server round trip", false, failure.what());
    }
}

static void checkRemoteBooks() {
    check("remote name is author and title", RemoteBookName::make("Vol de nuit", "Saint-Exupéry", {}) == "Saint-Exupéry - Vol de nuit.epub");
    check("remote name drops what FAT refuses",
          RemoteBookName::make("Qui? Quoi: \"rien\"/ tout.", "", {}) == "Qui Quoi rien tout.epub",
          RemoteBookName::make("Qui? Quoi: \"rien\"/ tout.", "", {}));
    check("remote name avoids taken names, ignoring case",
          RemoteBookName::make("Nuit", "", {"nuit.epub", "Nuit (2).EPUB"}) == "Nuit (3).epub");
    check("remote name is never empty", RemoteBookName::make("???", "", {}) == "Book.epub");
    std::string longTitle;
    for (int i = 0; i < 200; ++i) longTitle += "é";
    std::string cut = RemoteBookName::make(longTitle, "", {});
    check("remote name is cut between characters", cut.size() == 120 * 2 + 5 && g_utf8_validate(cut.c_str(), -1, nullptr));
    check("only epubs count as books", RemoteBookName::isBook("a.EPUB") && !RemoteBookName::isBook("a.pdf") && !RemoteBookName::isBook("._a.epub"));

    std::string listing =
        "<?xml version=\"1.0\"?><D:multistatus xmlns:D=\"DAV:\">"
        "<D:response><D:href>/dav/Books/</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>"
        "<D:response><D:href>/dav/Books/Saint-Exup%C3%A9ry%20-%20Vol%20de%20nuit.epub</D:href><D:propstat><D:prop><D:resourcetype/></D:prop></D:propstat></D:response>"
        "<D:response><D:href>https://example.org/dav/Books/Old/</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>"
        "</D:multistatus>";
    auto names = WebDav::fileNames(listing);
    check("listing gives files, decoded, without folders",
          names.size() == 1 && names[0] == "Saint-Exupéry - Vol de nuit.epub", names.empty() ? "" : names[0]);
    check("names escape for a url", WebDav::escape("Vol de nuit é.epub") == "Vol%20de%20nuit%20%C3%A9.epub");
}

/// Three devices and one server: the first sends its book, the second
/// fetches it, and the third, which has the same book already, only takes
/// note of the name. Needs AIREADER_SYNC_URL and a book on the command line.
static void checkLibraryServer(const std::string& folder, const std::string& epub) {
    const char* base = g_getenv("AIREADER_SYNC_URL");
    if (!base) return;
    SyncSettings settings{std::string(base) + "/library-" + std::to_string(getpid()),
                          g_getenv("AIREADER_SYNC_USER") ? g_getenv("AIREADER_SYNC_USER") : "",
                          g_getenv("AIREADER_SYNC_PASSWORD") ? g_getenv("AIREADER_SYNC_PASSWORD") : ""};
    auto device = [&](const std::string& name) {
        std::string home = Files::join(folder, name);
        g_setenv("AIREADER_HOME", home.c_str(), TRUE);
        Paths::prepare();
        auto database = std::make_unique<Database>(Paths::database());
        Migrations::migrate(*database);
        return database;
    };
    auto round = [&](Env& env) {
        LibrarySync::Outcome outcome = LibrarySync::exchange(settings, LibrarySync::gather(env));
        LibrarySync::record(env, outcome);
        return outcome;
    };

    auto first = device("first");
    Env a(*first, Paths::settings());
    Files::copy(epub, Files::join(Paths::books(), "livre.epub"));
    a.library.refresh(Paths::bookFolders());
    auto sent = round(a);
    check("first device sends its book", sent.error.empty() && sent.sent == 1 && sent.received.empty(), sent.error);
    auto quiet = round(a);
    check("first device then has nothing to do", quiet.error.empty() && quiet.sent == 0 && quiet.received.empty(), quiet.error);

    auto second = device("second");
    Env b(*second, Paths::settings());
    auto fetched = round(b);
    auto shelf = b.library.all();
    check("second device fetches it", fetched.error.empty() && fetched.received.size() == 1 && fetched.sent == 0
          && shelf.size() == 1 && !shelf[0].remoteName.empty() && Files::exists(shelf[0].path), fetched.error);
    check("second device then has nothing to do", round(b).received.empty() && round(b).sent == 0);
    b.library.remove(shelf[0].id);
    Files::remove(shelf[0].path);
    check("a book removed here is not fetched again", round(b).received.empty() && b.library.all().empty());

    auto third = device("third");
    Env c(*third, Paths::settings());
    Files::copy(epub, Files::join(Paths::books(), "same.epub"));
    c.library.refresh(Paths::bookFolders());
    auto matched = round(c);
    auto books = c.library.all();
    check("a book already here is recognised, not fetched or sent", matched.error.empty() && matched.received.empty()
          && matched.sent == 0 && books.size() == 1 && books[0].remoteName == sent.named[0].second, matched.error);
}

static void checkWordLists(const std::string& folder) {
    g_setenv("AIREADER_HOME", folder.c_str(), TRUE);
    Paths::prepare();
    Files::write(Files::join(Paths::dictionaries(), "mots.tsv"), "# mot\tsens\nchat\tкот\tкошка\nmaison\tдом\n");

    Database database(Files::join(folder, "library.sqlite3"));
    Migrations::migrate(database);
    DictionaryPacks packs(database);
    std::vector<std::string> errors;
    int added = packs.addFromFolder(errors);
    check("word list converts into a pack", added == 1 && errors.empty(), errors.empty() ? "" : errors[0]);
    check("scanning again adds nothing", packs.addFromFolder(errors) == 0);

    std::vector<DictionaryPack> converted;
    for (const auto& pack : packs.all()) if (!pack.isBundled()) converted.push_back(pack);
    DictionaryLookup lookup = DictionaryDatabase::shared().lookup("Chat,", converted);
    check("converted pack answers lookups", lookup.articles.size() == 1 && lookup.articles[0].senses.size() == 2 && lookup.articles[0].senses[0] == "кот",
          lookup.summary());
}

static std::string utf16le(const std::string& utf8) {
    glong written = 0;
    gunichar2* units = g_utf8_to_utf16(utf8.c_str(), -1, nullptr, &written, nullptr);
    std::string out = "\xFF\xFE";
    for (glong i = 0; i < written; ++i) {
        out += static_cast<char>(units[i] & 0xFF);
        out += static_cast<char>(units[i] >> 8);
    }
    g_free(units);
    return out;
}

static std::string gzipped(const std::string& data) {
    z_stream stream{};
    deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    std::string out(deflateBound(&stream, data.size()) + 32, '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    stream.avail_in = data.size();
    stream.next_out = reinterpret_cast<Bytef*>(&out[0]);
    stream.avail_out = out.size();
    deflate(&stream, Z_FINISH);
    out.resize(stream.total_out);
    deflateEnd(&stream);
    return out;
}

static std::string bigEndian(unsigned value, int bytes) {
    std::string out;
    for (int i = bytes - 1; i >= 0; --i) out += static_cast<char>((value >> (8 * i)) & 0xFF);
    return out;
}

static void checkConverters(const std::string& folder) {
    std::string dictionaries = Files::join(folder, "dictionaries");

    Files::write(Files::join(dictionaries, "lingvo.dsl"), utf16le(
        "#NAME \"Petit Lingvo\"\n#INDEX_LANGUAGE \"French\"\n#CONTENTS_LANGUAGE \"Russian\"\n\n"
        "pomme\n\t[m1][p]f[/p] яблоко[/m]\n\t[m1]{{comment}}[i]pomme de terre[/i] картофель[/m]\n"
        "poire\npoirier\n\t[m1]груша \\[плод\\][/m]\n"));

    std::string article = "h<b>кот</b><br/>кошка";
    Files::write(Files::join(dictionaries, "star.ifo"), "StarDict's dict ifo file\nversion=2.4.2\nbookname=Star FR\nwordcount=1\nlang=fr\ntargetlang=ru\n");
    Files::write(Files::join(dictionaries, "star.idx"), std::string("chat") + '\0' + bigEndian(0, 4) + bigEndian(article.size(), 4));
    Files::write(Files::join(dictionaries, "star.dict.dz"), gzipped(article));

    Files::write(Files::join(dictionaries, "open.xdxf"),
        "<?xml version=\"1.0\"?><xdxf lang_from=\"fra\" lang_to=\"rus\"><full_name>Open FR</full_name>"
        "<ar><k>chien</k>\nсобака\nпёс</ar><ar><k>loup</k><k>louve</k>\nволк</ar></xdxf>");

    auto sources = DictionaryFormats::sources({
        Files::join(dictionaries, "lingvo.dsl"), Files::join(dictionaries, "star.ifo"),
        Files::join(dictionaries, "star.idx"), Files::join(dictionaries, "star.dict.dz"), Files::join(dictionaries, "open.xdxf"),
    });
    bool grouped = sources.size() == 3;
    for (const auto& source : sources) {
        if (source.format == DictionaryFormat::StarDict && source.companions.size() != 2) grouped = false;
    }
    check("formats are recognized and StarDict files grouped", grouped, std::to_string(sources.size()) + " sources");
    check("dsl markup strips", DSLDictionaryReader::strip("[m1]{{x}}[i]pomme[/i] \\[plod\\][/m]") == "pomme [plod]",
          DSLDictionaryReader::strip("[m1]{{x}}[i]pomme[/i] \\[plod\\][/m]"));
    check("utf-16 files decode", TextFile::decode(utf16le("été")) == "été");

    Database database(Files::join(folder, "library.sqlite3"));
    DictionaryPacks packs(database);
    std::vector<std::string> errors;
    int added = packs.addFromFolder(errors);
    check("dsl, stardict and xdxf convert", added == 3 && errors.empty(), errors.empty() ? "" : Text::join(errors, " / "));

    std::vector<DictionaryPack> converted;
    std::string names;
    for (const auto& pack : packs.all()) {
        if (pack.isBundled()) continue;
        converted.push_back(pack);
        names += pack.name + " (" + pack.languages() + "); ";
    }
    check("converted packs carry their names and languages",
          Text::contains(names, "Petit Lingvo (fr → ru)") && Text::contains(names, "Star FR (fr → ru)") && Text::contains(names, "Open FR (fr → ru)"), names);

    DictionaryDatabase& dictionary = DictionaryDatabase::shared();
    DictionaryLookup pomme = dictionary.lookup("Pomme", converted);
    check("dsl article", pomme.articles.size() == 1 && pomme.articles[0].senses.size() == 2 && pomme.articles[0].senses[0] == "f яблоко", pomme.summary());
    DictionaryLookup poirier = dictionary.lookup("poirier", converted);
    check("dsl shared article", poirier.articles.size() == 1 && poirier.articles[0].senses[0] == "груша [плод]", poirier.summary());
    DictionaryLookup chat = dictionary.lookup("chat", converted);
    check("stardict article through gzip and html", chat.articles.size() == 2, chat.summary());
    bool starSenses = false;
    for (const auto& a : chat.articles) if (a.source == "Star FR" && a.senses.size() == 2 && a.senses[0] == "кот" && a.senses[1] == "кошка") starSenses = true;
    check("stardict senses", starSenses, chat.summary());
    DictionaryLookup louve = dictionary.lookup("louve", converted);
    check("xdxf second headword", louve.articles.size() == 1 && louve.articles[0].senses[0] == "волк", louve.summary());
    check("scanning again converts nothing", packs.addFromFolder(errors) == 0);
}

/// A dictionary picked from elsewhere on disk: StarDict's files travel
/// together, whichever of them was picked, and a known one is not added twice.
static void checkAddFile(const std::string& folder) {
    std::string elsewhere = Files::join(folder, "elsewhere");
    Files::ensureDirectory(elsewhere);
    std::string article = "h<b>верба</b>";
    Files::write(Files::join(elsewhere, "saule.ifo"), "StarDict's dict ifo file\nversion=2.4.2\nbookname=Saule FR\nwordcount=1\nlang=fr\ntargetlang=ru\n");
    Files::write(Files::join(elsewhere, "saule.idx"), std::string("saule") + '\0' + bigEndian(0, 4) + bigEndian(article.size(), 4));
    Files::write(Files::join(elsewhere, "saule.dict.dz"), gzipped(article));
    Files::write(Files::join(elsewhere, "notes.txt"), "not a dictionary at all");

    Database database(Files::join(folder, "library.sqlite3"));
    DictionaryPacks packs(database);
    std::vector<std::string> errors;
    int added = packs.addFile(Files::join(elsewhere, "saule.idx"), errors);
    check("a picked stardict companion brings the whole set", added == 1 && errors.empty(), errors.empty() ? "" : errors[0]);
    check("picked files are copied into the folder",
          Files::exists(Files::join(Paths::dictionaries(), "saule.ifo")) && Files::exists(Files::join(Paths::dictionaries(), "saule.dict.dz")));
    check("picking it again adds nothing", packs.addFile(Files::join(elsewhere, "saule.ifo"), errors) == 0 && errors.empty());
    std::vector<DictionaryPack> converted;
    for (const auto& pack : packs.all()) if (pack.name == "Saule FR") converted.push_back(pack);
    DictionaryLookup saule = DictionaryDatabase::shared().lookup("saule", converted);
    check("picked pack answers lookups", saule.articles.size() == 1 && saule.articles[0].senses[0] == "верба", saule.summary());
    errors.clear();
    check("a file in no known format is refused", packs.addFile(Files::join(elsewhere, "notes.txt"), errors) == 0 && !errors.empty(),
          errors.empty() ? "" : errors[0]);
}

static void checkBundledDictionary() {
    std::string path = Paths::bundledDictionary();
    if (!Files::exists(path)) {
        std::printf("skip  bundled dictionary (set AIREADER_DATA_DIR to the folder holding dictionary.sqlite3)\n");
        return;
    }
    DictionaryPack bundled;
    bundled.name = "Bundled";
    DictionaryLookup lookup = DictionaryDatabase::shared().lookup("maisons", {bundled});
    check("bundled dictionary resolves a form", !lookup.forms.empty() && lookup.forms[0].lemma == "maison", lookup.summary());
    check("bundled dictionary has an article", !lookup.articles.empty() && lookup.articles[0].lemma == "maison");
    auto entry = DictionaryDatabase::shared().articlesFor("Maison", {bundled});
    check("entry under a headword", !entry.empty() && entry[0].lemma == "maison" && !entry[0].senses.empty() && entry[0].source == "Bundled",
          entry.empty() ? "(none)" : entry[0].senses[0]);
    check("a form is not the headword's entry", DictionaryDatabase::shared().articlesFor("maisons", {bundled}).empty());
    check("a guessed lemma has no entry", DictionaryDatabase::shared().articlesFor("Xylozoptrix", {bundled}).empty());

    AiSettings mock{"mock://ai", "", "", "mock-medium"};
    WordExplanation explanation = WordExplainer::explain("maisons", "Les maisons étaient vieilles.", mock, {{}, {bundled}, {}});
    check("explainer answers through the mock", explanation.lemma == "maison" && !explanation.guessed, explanation.meaning);
    WordExplanation unknown = WordExplainer::explain("Xylozoptrix", "Le Xylozoptrix dort.", mock, {{}, {bundled}, {}});
    check("explainer marks a miss as guessed", unknown.guessed);
    check("mock model list", ChatApi::models(mock).size() == 2);

    // A corpus of two books, one handed over by the reader, the other
    // unreadable; the search stops at the reader's position in the first.
    Book first;
    first.id = 1;
    first.title = "Tome 1";
    Book second;
    second.id = 2;
    second.title = "Tome 2";
    second.path = "/nonexistent/tome2.epub";
    auto corpus = std::make_shared<BookCorpus>(std::vector<Book>{first, second});
    corpus->provide(1, {"Meaulnes arriva. Meaulnes parla.", "Meaulnes partit."});
    check("corpus searches the books it has", corpus->search("meaulnes", 10).size() == 3 && corpus->search("meaulnes", 10)[2].chapter == 1);
    check("corpus reports a book it cannot read", corpus->errors().size() == 1 && Text::startsWith(corpus->errors()[0], "Tome 2"));
    BookPosition upTo{1, 0, static_cast<int>(std::string("Meaulnes arriva. ").size())};
    auto seen = corpus->search("meaulnes", 10, upTo);
    check("corpus keeps the model to what was read", seen.size() == 1 && seen[0].offset == 0 && seen[0].bookTitle == "Tome 1");

    ToolRunner::Tools tools{{corpus, upTo}, {}, {}};
    std::vector<ChatMessage> xray = XRayPrompt::messages("Nobody", corpus->search("Nobody", 5, upTo), true);
    ChatMessage answer = ToolRunner::converse(mock, xray, Json(std::vector<Json>{SearchTool::tool()}), false, tools);
    check("tool runner answers a search call", xray.size() == 4 && xray[3].role == "tool" && Text::contains(*xray[3].content, "No passage")
          && Text::contains(answer.content.value_or(""), "не встречается"), answer.content.value_or(""));
    std::vector<ChatMessage> found = XRayPrompt::messages("Meaulnes", corpus->search("Meaulnes", 5, upTo), true);
    ChatMessage known = ToolRunner::converse(mock, found, Json(std::vector<Json>{SearchTool::tool()}), false, tools);
    check("x-ray answers from the passages read so far", Text::contains(known.content.value_or(""), "1 отрывках"), known.content.value_or(""));
}

/// Draws the first page of the first chapter the way the reader does, so the
/// typography can be looked at.
static void renderPage(const std::string& epub, const std::string& out, int chapterIndex, int pageIndex) {
    auto document = EpubLoader::load(epub);
    if (!document || chapterIndex >= static_cast<int>(document->chapters.size())) return;
    const int width = 600, height = 800, margin = 32;
    PangoContext* context = pango_font_map_create_context(pango_cairo_font_map_get_default());
    Illustrations::install(context);
    ReadingStyle style;
    PageLayout layout = Paginator::paginate(context, document->chapters[chapterIndex], style,
                                            width - 2 * margin, height - 2 * margin, 1.0, document->language);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    if (pageIndex >= static_cast<int>(layout.pages.size())) pageIndex = 0;
    const Page& page = layout.pages[pageIndex];
    cairo_rectangle(cr, margin, margin, width - 2 * margin, page.height / PANGO_SCALE + 1);
    cairo_clip(cr);
    cairo_translate(cr, margin, margin - page.top / static_cast<double>(PANGO_SCALE));
    pango_cairo_show_layout(cr, layout.layout);
    cairo_destroy(cr);
    cairo_surface_write_to_png(surface, out.c_str());
    cairo_surface_destroy(surface);
    g_object_unref(context);
    std::printf("      rendered page %d of %zu to %s\n", pageIndex + 1, layout.pages.size(), out.c_str());
}

static void checkEpub(const std::string& path) {
    std::string error;
    auto metadata = EpubLoader::metadata(path, &error);
    check("epub metadata", metadata.has_value(), error.empty() ? (metadata ? metadata->title : "") : error);
    auto document = EpubLoader::load(path, &error);
    check("epub loads chapters", document && !document->chapters.empty(), document ? std::to_string(document->chapters.size()) + " chapters" : error);
    if (document) {
        size_t total = 0;
        for (const auto& chapter : document->chapters) total += chapter.text.size();
        std::printf("      language %s, %zu bytes of text, cover %s\n",
                    document->language.c_str(), total, EpubLoader::cover(path) ? "yes" : "no");
        bool inRange = true;
        for (const auto& entry : document->contents) {
            inRange = inRange && entry.chapter >= 0 && entry.chapter < static_cast<int>(document->chapters.size())
                && entry.offset >= 0 && entry.offset <= static_cast<int>(document->chapters[entry.chapter].text.size());
        }
        check("epub has a table of contents pointing into its chapters", !document->contents.empty() && inRange,
              std::to_string(document->contents.size()) + " entries, first “" + (document->contents.empty() ? "" : document->contents[0].title) + "”");
        // The corpus reads the book without its pictures; chapter numbers
        // must still agree with the reader's, or a search hit lands wrong.
        auto text = EpubLoader::load(path, nullptr, false);
        bool same = text && text->chapters.size() == document->chapters.size();
        for (size_t i = 0; same && i < text->chapters.size(); ++i) same = text->chapters[i].text == document->chapters[i].text;
        check("epub loads the same chapters without pictures", same);

        Book book;
        book.id = 1;
        book.title = metadata ? metadata->title : "Book";
        book.path = path;
        BookCorpus corpus({book});
        auto hits = corpus.search("le", 5);
        check("corpus searches the epub from disk", hits.size() == 5 && corpus.errors().empty(), hits.empty() ? "" : hits[0].excerpt);
    }
}

int main(int argc, char** argv) {
    g_thread_init(nullptr);
    ChatApi::initialize();
    checkJson();
    checkEpubPackage();
    checkEpubNavigation();
    checkHtmlText();
    checkLanguage();
    checkDictionaryLookup();
    checkMock();
    checkQuirks();
    checkChatPrompt();
    checkWebSearch();
    checkBookSearch();
    checkWordContext();
    checkCards();
    checkPaginator();

    char pattern[] = "/tmp/aireader-check-XXXXXX";
    const char* folder = mkdtemp(pattern);
    std::string scratch = folder ? folder : "/tmp";
    checkReadingPlace();
    checkRemoteBooks();
    checkSyncDocument();
    checkDatabase(scratch);
    checkSyncStore(scratch);
    checkSyncServer(scratch);
    checkWordLists(Files::join(scratch, "home"));
    checkConverters(Files::join(scratch, "home"));
    checkAddFile(Files::join(scratch, "home"));
    checkBundledDictionary();
    if (argc > 1) checkLibraryServer(Files::join(scratch, "library"), argv[1]);
    if (argc > 1) checkEpub(argv[1]);
    if (argc > 2) renderPage(argv[1], argv[2], argc > 3 ? std::atoi(argv[3]) : 0, argc > 4 ? std::atoi(argv[4]) : 0);

    std::printf("%s\n", failures ? "FAILED" : "all checks passed");
    return failures ? 1 : 0;
}
