// Runs the pure logic from the command line: prompt building, mock replies,
// dictionary formatting, parameter negotiation, EPUB reading, pagination and
// the database. No display needed.
//
//   AIREADER_DATA_DIR=../AIReader/AIReader/Resources ./aireader-check [book.epub [page.png [chapter [page]]]]

#include "../src/Domain/AI/ChatPrompt.hpp"
#include "../src/Domain/AI/ContextTool.hpp"
#include "../src/Domain/AI/DictionaryTool.hpp"
#include "../src/Domain/AI/ExplanationPrompt.hpp"
#include "../src/Domain/AI/GlossaryPrompt.hpp"
#include "../src/Domain/AI/MockAI.hpp"
#include "../src/Domain/AI/RequestQuirks.hpp"
#include "../src/Services/RequestQuirkStore.hpp"
#include "../src/Domain/AI/SearchTool.hpp"
#include "../src/Domain/AI/XRayPrompt.hpp"
#include "../src/Domain/AI/WebSearchTool.hpp"
#include "../src/Domain/AI/WordExplanation.hpp"
#include "../src/Domain/Books/BookDocument.hpp"
#include "../src/Domain/Books/EpubNavigation.hpp"
#include "../src/Domain/Books/EpubPackage.hpp"
#include "../src/Domain/Books/Fb2Book.hpp"
#include "../src/Domain/Books/HtmlBook.hpp"
#include "../src/Domain/Books/HtmlText.hpp"
#include "../src/Domain/Books/LanguageDetector.hpp"
#include "../src/Domain/Books/TextBook.hpp"
#include "../src/Domain/Cards/AnkiExport.hpp"
#include "../src/Domain/Cards/Card.hpp"
#include "../src/Domain/Cards/MatchRound.hpp"
#include "../src/Domain/Dictionary/DictionaryLookup.hpp"
#include "../src/Domain/Dictionary/WordNormalizer.hpp"
#include "../src/Domain/Formats/DSLDictionaryReader.hpp"
#include "../src/Domain/Formats/DictionaryFormat.hpp"
#include "../src/Support/TextFile.hpp"
#include "../src/Domain/Reading/Illustrations.hpp"
#include "../src/Domain/Reading/BookWords.hpp"
#include "../src/Domain/Reading/Paginator.hpp"
#include "../src/Domain/Reading/WordContext.hpp"
#include "../src/Domain/Books/ReadingPlace.hpp"
#include "../src/Domain/Search/BookSearch.hpp"
#include "Domain/Sync/SyncDocument.hpp"
#include "Domain/Sync/SyncParts.hpp"
#include "../src/Domain/Books/BookKey.hpp"
#include "../src/Domain/Books/RemoteBookName.hpp"
#include "../src/Services/BookCorpus.hpp"
#include "../src/Services/BookImporter.hpp"
#include "../src/Services/ChatApi.hpp"
#include "../src/Services/Database.hpp"
#include "../src/Services/DictionaryDatabase.hpp"
#include "../src/Services/DictionaryPacks.hpp"
#include "../src/Services/Env.hpp"
#include "../src/Services/EpubLoader.hpp"
#include "../src/Services/EpubWriter.hpp"
#include "../src/Services/Glossary.hpp"
#include "../src/Services/LibraryStore.hpp"
#include "../src/Services/LibrarySync.hpp"
#include "../src/Services/LookupCache.hpp"
#include "../src/Services/Migrations.hpp"
#include "../src/Services/Paths.hpp"
#include "../src/Services/PdfImporter.hpp"
#include "../src/Services/Sync.hpp"
#include "../src/Services/SyncStore.hpp"
#include "../src/Services/ToolRunner.hpp"
#include "../src/Services/WebDav.hpp"
#include "../src/Services/WordExplainer.hpp"
#include "../src/Support/Charset.hpp"
#include "../src/Support/Files.hpp"
#include "../src/Support/PdfPageText.hpp"
#include "../src/Support/XmlScanner.hpp"
#include "../src/Support/ZipArchive.hpp"
#include "../src/Support/ZipWriter.hpp"
#include "Support/Json.hpp"
#include "../src/Support/Text.hpp"

#include <pango/pangocairo.h>
#include <zlib.h>

#include <algorithm>
#include <clocale>
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
        ChatMessage::system(ExplanationPrompt::system("Russian")),
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
        ChatMessage::system(ExplanationPrompt::system("Russian")),
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
    auto asked = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Поищи Alain-Fournier"}}, "Russian");
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
    check("quirk names go both ways", requestQuirkCalled(requestQuirkName(RequestQuirk::CompletionTokens)) == RequestQuirk::CompletionTokens
          && !requestQuirkCalled("nothing"));

    // What a run learned is there for the next one, once a file is named.
    char pattern[] = "/tmp/aireader-quirks-XXXXXX";
    std::string file = std::string(mkdtemp(pattern)) + "/request-quirks.txt";
    RequestQuirkStore& store = RequestQuirkStore::shared();
    store.keepIn(file);
    store.learn(RequestQuirk::CompletionTokens, "https://api.example/v1|luna");
    store.learn(RequestQuirk::NoReasoning, "https://api.example/v1|luna");
    store.learn(RequestQuirk::DefaultTemperature, "https://api.example/v1|other");
    store.keepIn(file);
    check("learned quirks outlast the run", store.quirks("https://api.example/v1|luna").size() == 2
          && store.quirks("https://api.example/v1|other").count(RequestQuirk::DefaultTemperature) && store.quirks("m").empty(),
          Files::read(file).value_or(""));
    store.forget("https://api.example/v1|luna");
    store.keepIn(file);
    check("a model's quirks can be forgotten", store.quirks("https://api.example/v1|luna").empty() && store.quirks("https://api.example/v1|other").size() == 1);
    store.keepIn("");
}

static void checkChatPrompt() {
    auto messages = ChatPrompt::messages(ChatPrompt::pageContext("Page text."), {{true, "Q1"}, {false, "A1"}, {true, "Q2"}}, "Russian");
    check("chat sends the page once", messages.size() == 4 && Text::contains(*messages[1].content, "Page text.") && *messages[3].content == "Q2");
    check("chat keeps the model's turns", messages[2].role == "assistant" && *messages[2].content == "A1");
    check("chat answers in the chosen language", Text::contains(*ChatPrompt::messages("", {{true, "Q"}}, "English")[0].content, "«English»")
          && Text::contains(ExplanationPrompt::system("English"), "«English»") && !Text::contains(ExplanationPrompt::system("English"), "по-русски"));
    WordExplanation explanation{"maison", "мн. ч.", "дом", false, 0.9};
    std::string word = ChatPrompt::wordContext("maisons", "Les maisons.", explanation);
    check("a word seeds a conversation with its explanation", Text::contains(word, "Слово: maisons") && Text::contains(word, "Объяснение: дом"));
    ChatMessage echo = MockAI::reply(ChatPrompt::messages(word, {{true, "Почему?"}}, "Russian"));
    check("mock tells a seeded conversation from a lookup", echo.toolCalls.empty() && Text::contains(echo.content.value_or(""), "«Почему?»"),
          echo.content.value_or("(tool call)"));

    // Asked to find something, the mock searches the book the way a model would.
    auto asked = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Найди Meaulnes"}}, "Russian");
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
    auto meaning = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Что значит maison?"}}, "Russian");
    ChatMessage lookup = MockAI::reply(meaning);
    check("mock chat calls lookup_dictionary", lookup.toolCalls.size() == 1 && lookup.toolCalls[0].name == DictionaryTool::toolName
          && DictionaryTool::word(lookup.toolCalls[0].arguments) == "maison");
    meaning.push_back(lookup);
    meaning.push_back(ChatMessage::toolResult("Dictionary results for “maison”:\n- maison [noun]: дом, здание; семья", lookup.toolCalls[0].id));
    check("mock chat answers from the article", Text::contains(MockAI::reply(meaning).content.value_or(""), "«maison» — дом, здание"));
    check("chat offers the dictionary, the search and the text around", ChatPrompt::tools().size() == 3);
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
    check("tools carry the search and the text around", ExplanationPrompt::tools().size() == 3
          && ExplanationPrompt::tools().at(1).at("function").at("name").string() == SearchTool::toolName
          && ExplanationPrompt::tools().at(2).at("function").at("name").string() == ContextTool::toolName);

    // The X-ray asks once more when it has nothing, then reports the count.
    auto xray = XRayPrompt::messages("Meaulnes", {}, false, "Russian");
    ChatMessage again = MockAI::reply(xray);
    check("mock x-ray searches when given no passages", again.toolCalls.size() == 1 && SearchTool::query(again.toolCalls[0].arguments) == "meaulnes");
    xray.push_back(again);
    xray.push_back(ChatMessage::toolResult(SearchTool::summary("meaulnes", {hit, hit, hit}, false), again.toolCalls[0].id));
    check("mock x-ray answers from the passages", Text::contains(MockAI::reply(xray).content.value_or(""), "3 отрывках"));
}

static void checkContextTool() {
    std::string chapter;
    for (int i = 0; i < 60; ++i) chapter += "Phrase " + std::to_string(i) + " dit une chose assez longue pour compter. ";
    int start = static_cast<int>(chapter.find("Phrase 30 "));
    int end = static_cast<int>(chapter.find("Phrase 31 "));
    BookPassage window{1, 0, start, end};
    std::string before = ContextTool::read(ContextTool::Direction::Before, chapter, window);
    check("context reads back from a sentence's start", Text::startsWith(before, "Before it in the book:\nPhrase ")
          && Text::endsWith(before, "Phrase 29 dit une chose assez longue pour compter.")
          && window.start < start && start - window.start <= BookSearch::step + BookSearch::reach, before);
    std::string after = ContextTool::read(ContextTool::Direction::After, chapter, window);
    check("context reads on to a sentence's end", Text::startsWith(after, "After it in the book:\nPhrase 31 ")
          && Text::endsWith(after, "compter.") && window.end > end, after);
    int reached = window.start;
    ContextTool::read(ContextTool::Direction::Before, chapter, window);
    check("each call reads further", window.start < reached);
    std::string last;
    for (int i = 0; i < 4; ++i) last = ContextTool::read(ContextTool::Direction::Before, chapter, window);
    check("context stops at the chapter's start", window.start == 0 && last == "Nothing comes before it: the chapter begins there.", last);
    BookPassage early{1, 0, static_cast<int>(chapter.find("Phrase 2 ")), static_cast<int>(chapter.find("Phrase 3 "))};
    std::string first = ContextTool::read(ContextTool::Direction::Before, chapter, early);
    check("context says when it reached the start", Text::startsWith(first, "Before it in the book:\nPhrase 0 ")
          && Text::endsWith(first, "(The chapter begins here.)"), first);
    std::string unbroken(3000, 'x');
    BookPassage middle{1, 0, 1500, 1510};
    ContextTool::read(ContextTool::Direction::After, unbroken, middle);
    check("a text with no sentences is still read a step at a time", middle.end > 1510 && middle.end <= 1510 + BookSearch::step);
    BookPassage nearEnd{1, 0, 2000, 2100};
    check("a step near the end takes the rest", Text::endsWith(ContextTool::read(ContextTool::Direction::After, unbroken, nearEnd), "(The chapter ends here.)")
          && nearEnd.end == 3000);

    check("context tool takes a direction", ContextTool::direction(R"({"direction":"after"})") == ContextTool::Direction::After
          && !ContextTool::direction(R"({"direction":"later"})")
          && ContextTool::tool().at("function").at("parameters").at("properties").at("direction").at("enum").size() == 2);

    // Asked what came before, the mock reads around the page, and the runner
    // answers from the passage the reader is on.
    Book book;
    book.id = 1;
    auto corpus = std::make_shared<BookCorpus>(std::vector<Book>{book});
    corpus->provide(1, {"Intro.", chapter});
    ReadingScope scope{corpus, BookPosition{1, 1, end}, BookPassage{1, 1, start, end}};
    AiSettings mock{"mock://ai", "", "", "mock-medium"};
    auto asked = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Раньше что было?"}}, "Russian");
    ChatMessage answer = ToolRunner::converse(mock, asked, ChatPrompt::tools(), false, {scope, {}, {}});
    check("mock chat reads before the passage", asked.size() == 4 && asked[2].toolCalls.size() == 1
          && asked[2].toolCalls[0].name == ContextTool::toolName && Text::startsWith(*asked[3].content, "Before it in the book:")
          && Text::contains(answer.content.value_or(""), "перед этим местом в книге — «Phrase"), answer.content.value_or(""));
    auto later = ChatPrompt::messages(ChatPrompt::pageContext("Page."), {{true, "Дальше?"}}, "Russian");
    ToolRunner::converse(mock, later, ChatPrompt::tools(), false, {ReadingScope{corpus, std::nullopt, std::nullopt}, {}, {}});
    check("without a passage there is nothing around", later.size() == 4 && *later[3].content == ContextTool::nothingAround);
}

static std::string pdfStream(const std::string& dictionary, const std::string& data) {
    return "<< " + dictionary + " /Length " + std::to_string(data.size()) + " >>\nstream\n" + data + "\nendstream";
}

static std::string zlibbed(const std::string& data) {
    uLongf size = compressBound(data.size());
    std::string out(size, '\0');
    compress(reinterpret_cast<Bytef*>(&out[0]), &size, reinterpret_cast<const Bytef*>(data.data()), data.size());
    out.resize(size);
    return out;
}

/// A PDF of its objects' bodies, with no cross-reference table: the reader
/// finds objects by scanning, so it should not need one.
static std::string pdfFile(const std::map<int, std::string>& objects, const std::string& trailer) {
    std::string out = "%PDF-1.5\n%\xE2\xE3\xCF\xD3\n";
    for (const auto& object : objects) out += std::to_string(object.first) + " 0 obj\n" + object.second + "\nendobj\n";
    return out + "trailer\n" + trailer + "\n%%EOF\n";
}

/// Four pages with what a printed book's PDF has: running heads, page
/// numbers, a heading, first-line indents, a word hyphenated at a line's
/// end, a paragraph cut by a page break, and two bookmarks.
static std::string samplePdf() {
    std::map<int, std::string> objects;
    objects[1] = "<< /Type /Catalog /Pages 2 0 R /Outlines 20 0 R /Lang (fr-FR) /Names << /Dests << /Names [(suite) [4 0 R /Fit]] >> >> >>";
    // Resources and the media box are inherited from the page tree.
    objects[2] = "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R 6 0 R] /Count 4 /MediaBox [0 0 612 792] "
                 "/Resources << /Font << /F1 10 0 R /F2 11 0 R >> /XObject << /X1 15 0 R >> >> >>";
    objects[3] = "<< /Type /Page /Parent 2 0 R /Contents 7 0 R >>";
    objects[4] = "<< /Type /Page /Parent 2 0 R /Contents 8 0 R >>";
    objects[5] = "<< /Type /Page /Parent 2 0 R /Contents [9 0 R] >>";
    objects[6] = "<< /Type /Page /Parent 2 0 R /Contents 16 0 R >>";
    const std::string head = "BT /F1 9 Tf 1 0 0 1 72 770 Tm (Mon Livre) Tj ET\n";
    auto folio = [](const std::string& number) { return "BT /F1 9 Tf 1 0 0 1 300 40 Tm (" + number + ") Tj ET\n"; };
    objects[7] = pdfStream("/Filter /FlateDecode", zlibbed(head + R"(BT /F1 20 Tf 1 0 0 1 72 740 Tm (Chapitre premier) Tj ET
BT /F1 12 Tf 1 0 0 1 90 700 Tm (Il \351tait une fois un chat qui ai-) Tj
1 0 0 1 72 686 Tm (mait les livres et la musique.) Tj
1 0 0 1 90 672 Tm (Le chat lisait chaque soir, \340 la lumi\350re d\222une) Tj
-18 -14 Td (bougie, des histoires de) Tj ET
)" + folio("1")));
    // A word split by a little kerning stays whole; a wide gap is a space.
    objects[8] = pdfStream("", head + R"(BT /F1 12 Tf 1 0 0 1 72 740 Tm (pirates et de magiciens. Ses amis le trouvaient raf\001n\351.) Tj
1 0 0 1 90 726 Tm [(Un) -280 (jour,) -280 (le) -280 (ch) -20 (at) -280 (partit.)] TJ
/F2 12 Tf 1 0 0 1 90 712 Tm <0001000200030004> Tj ET
)" + folio("2"));
    // An inline image whose bytes would show an X if read as content.
    objects[9] = pdfStream("", head + "q /X1 Do Q\nBI /W 6 /H 1 /BPC 8 /CS /G ID (X) Tj EI\n"
                           "BT /F1 12 Tf 1 0 0 1 90 726 Tm (Fin du livre.) Tj ET\n" + folio("3"));
    objects[16] = pdfStream("", head + "BT /F1 12 Tf 1 0 0 1 90 740 Tm (Derni\\350re page.) Tj ET\n" + folio("iv"));
    objects[11] = "<< /Type /Font /Subtype /Type0 /BaseFont /Sample /Encoding /Identity-H /DescendantFonts [12 0 R] /ToUnicode 14 0 R >>";
    objects[12] = "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /Sample /W [1 [600 600 600 500]] >>";
    objects[14] = pdfStream("", "/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n"
                                "1 begincodespacerange <0000> <FFFF> endcodespacerange\n"
                                "1 beginbfrange <0001> <0003> <0041> endbfrange\n"
                                "1 beginbfchar <0004> <00E9> endbfchar\nendcmap end end");
    objects[15] = pdfStream("/Type /XObject /Subtype /Form /BBox [0 0 612 792] /Matrix [1 0 0 1 72 740]",
                            "BT /F1 12 Tf 0 0 Td (Texte dans une forme.) Tj ET");
    objects[20] = "<< /Type /Outlines /First 21 0 R /Last 22 0 R /Count 2 >>";
    objects[21] = "<< /Title (D\\351but) /Parent 20 0 R /Next 22 0 R /Dest [3 0 R /XYZ 0 792 0] >>";
    objects[22] = "<< /Title <FEFF00530075006900740065> /Parent 20 0 R /Prev 21 0 R /Dest (suite) >>";
    // The simple font and its encoding live in an object stream.
    std::string font = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding 13 0 R >>";
    std::string encoding = "<< /Type /Encoding /BaseEncoding /WinAnsiEncoding /Differences [1 /fi] >>";
    std::string index = "10 0 13 " + std::to_string(font.size() + 1) + " ";
    objects[30] = pdfStream("/Type /ObjStm /N 2 /First " + std::to_string(index.size()) + " /Filter /FlateDecode",
                            zlibbed(index + font + " " + encoding));
    objects[40] = "<< /Title <FEFF004D006F006E0020004C0069007600720065> /Author (Jean Dupont) >>";
    return pdfFile(objects, "<< /Root 1 0 R /Info 40 0 R /Size 41 >>");
}

static void checkPdf(const std::string& folder) {
    std::string pdf = samplePdf();
    PdfDocument document(pdf);
    check("pdf pages come from the page tree", document.pages().size() == 4);
    check("pdf information reads UTF-16 and PDFDocEncoding", document.info("Title") == "Mon Livre" && document.info("Author") == "Jean Dupont");
    auto outline = document.outline();
    check("pdf bookmarks, direct and named, find their pages", outline.size() == 2 && outline[0].title == "Début" && outline[0].page == 0
          && outline[1].title == "Suite" && outline[1].page == 1);
    PdfPageText::FontCache fonts;
    auto second = PdfPageText::lines(document, document.pages()[1], fonts);
    check("pdf text: encodings, a ligature, kerning and a two-byte font", second.size() == 5
          && second[1].text == "pirates et de magiciens. Ses amis le trouvaient raffiné."
          && second[2].text == "Un jour, le chat partit." && second[3].text == "ABCé",
          second.size() > 3 ? second[1].text + " | " + second[2].text + " | " + second[3].text : std::to_string(second.size()));
    check("pdf lines know where they are", second.size() == 5 && std::abs(second[2].left - 90) < 0.01 && std::abs(second[2].y - 66) < 0.01
          && std::abs(second[2].size - 12) < 0.01);

    std::string epub = Files::join(folder, "sample.epub");
    Files::ensureDirectory(folder);
    Files::write(epub, PdfImporter::epub(pdf, "fallback"));
    std::string error;
    auto metadata = EpubLoader::metadata(epub, &error);
    check("pdf epub names the book and its language", metadata && metadata->title == "Mon Livre" && metadata->author == "Jean Dupont"
          && metadata->language == "fr", error);
    auto book = EpubLoader::load(epub, &error, false);
    bool loaded = book && book->chapters.size() == 2;
    check("pdf epub has a chapter per bookmark", loaded && book->contents.size() == 2 && book->contents[0].title == "Début"
          && book->contents[1].title == "Suite", book ? std::to_string(book->chapters.size()) : error);
    std::string first = loaded ? book->chapters[0].text : "";
    std::string rest = loaded ? book->chapters[1].text : "";
    check("pdf paragraphs: heads and folios dropped, a hyphenated word mended, a page break undone",
          first == "Chapitre premier\nIl était une fois un chat qui aimait les livres et la musique.\n"
                   "Le chat lisait chaque soir, à la lumière d’une bougie, des histoires de pirates et de magiciens. "
                   "Ses amis le trouvaient raffiné.", first);
    check("pdf text in a form is read, an inline image is not", rest == "Un jour, le chat partit.\nABCé\nTexte dans une forme.\nFin du livre.\nDernière page.", rest);
    check("pdf heading stays a heading", loaded && !book->chapters[0].spans.empty() && book->chapters[0].spans[0].kind == TextSpan::Kind::Heading);

    auto refused = [](const std::string& data) {
        try {
            PdfImporter::epub(data, "x");
        } catch (const std::exception& failure) {
            return std::string(failure.what());
        }
        return std::string();
    };
    check("not a pdf", Text::contains(refused("hello"), "not a PDF"));
    check("an encrypted pdf is refused", Text::contains(refused(pdfFile({{1, "<< /Type /Catalog /Pages 2 0 R >>"},
        {2, "<< /Type /Pages /Kids [3 0 R] >>"}, {3, "<< /Type /Page >>"}, {4, "<< /Filter /Standard >>"}},
        "<< /Root 1 0 R /Encrypt 4 0 R >>")), "encrypted"));
    std::string scan = refused(pdfFile({{1, "<< /Type /Catalog /Pages 2 0 R >>"}, {2, "<< /Type /Pages /Kids [3 0 R] >>"},
                                        {3, "<< /Type /Page /Contents 4 0 R >>"}, {4, pdfStream("", "q 100 0 0 100 0 0 cm /Im1 Do Q")}},
                                       "<< /Root 1 0 R >>"));
    check("a scan without text is sent to ScanTool", Text::contains(scan, "ScanTool"), scan);
}

/// A book's chapters on one line: `title = #heading / paragraph || …`.
static std::string draftOutline(const BookDraft& book) {
    std::string out;
    for (size_t i = 0; i < book.chapters.size(); ++i) {
        if (i) out += " || ";
        out += book.chapters[i].title + " = ";
        for (size_t j = 0; j < book.chapters[i].paragraphs.size(); ++j) {
            const auto& paragraph = book.chapters[i].paragraphs[j];
            out += (j ? " / " : "") + std::string(paragraph.heading ? "#" : "") + paragraph.text;
        }
    }
    return out;
}

static std::string repeated(const std::string& text, int count) {
    std::string out;
    for (int i = 0; i < count; ++i) out += text;
    return out;
}

/// The plain-text reader: the same cases as the web app's
/// `tools/checks/TextBook.mjs`, since the apps must read a text file alike.
static void checkTextBook() {
    std::string gutenberg = Text::join({
        "The Project Gutenberg eBook of A Small Book", "", "Title: A   Small\tBook", "", "Author: Jane  Doe", "",
        "*** START OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***", "", "[Illustration]", "",
        "CHAPTER I.", "Down the Hole", "", "Alice was beginning to get very", "tired of sitting.", "",
        "CHAPTER II.", "Up the Hill", "", "So she was considering.", "",
        "End of the Project Gutenberg EBook of A Small Book, by Jane Doe", "",
        "*** END OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***", "", "The license.", "",
    }, "\r\n");
    std::string numbered = Text::join({
        "", "  A preface that is wrapped", "\tacross two lines.", "", "I", "", "The first.", "", "II.", "",
        "XIV", "", "12", "", "Twelve.", "", "iv", "", "1.5", "", "DID", "", "1234", "", "MMMCMXCIX.", "", "",
    }, "\n");
    std::string lines = Text::join({
        "A first line", "Chapter 1", "It was a dark night.", "CHAPTER TWO: THE STORM", "Chapter", "The wind howled.",
        "Chapters are not headings.", "Chapter: one", "",
    }, "\n");
    std::string russian = Text::join({
        "Предисловие.", "ЧАСТЬ ПЕРВАЯ", "I", "Мой дядя.", "II", "Книга лежала на столе.", "Глава первая", "ЧАСТЬ ВТОРАЯ", "I", "Он уехал.",
        "Пролог", "Пришла весна.", "Розділ 3", "Кінець.",
    }, "\r");
    // A list of contents running into the book: the last two headings name the chapter.
    std::string parts = Text::join({
        "Contents", "BOOK ONE: 1805", "CHAPTER I", "CHAPTER II", "BOOK TWO: 1805", "CHAPTER I", "BOOK ONE: 1805", "CHAPTER I", "Well, Prince.",
        "CHAPTER II", "Anna Pavlovna.", "BOOK TWO: 1805", "CHAPTER I", "In October.",
    }, "\n\n");
    std::vector<std::string> opening = {
        "XIV", "12.", "MMMCMXCIX", "CHAPTER I.", "Chapter 12: The Return", "Book IV—The Fall", "Part 3-a", "Глава 1", "ГЛАВА ПЕРВАЯ",
        "BOOK ONE: 1805", "Prologue", "Пролог", "Epilogue.", "Chapter:", "Chapter\u00a09", "\u00a0Chapter 9",
    };
    std::vector<std::string> notOpening = {
        "1234", "DID", "CIVIL", "IIII", "iv", "1.5", "Part of me wanted to stay.", "Книга лежала на столе.", "Chapter One", "Глава первая",
        "Chapter: one", "Chapter;", "Chapter 1805", "Chapter iv", "Chapters are not headings.", "Book the First",
    };
    std::string french = Text::join({"CHAPITRE PREMIER", "", "Il dit\u00a0: «\u00a0Bonjour\u00a0»", "", "Première partie", "", "\u00a0\u00a0", "", "ÉPILOGUE", "", "\u00a0Fin\u00a0"}, "\n");
    std::string lengths = Text::join({
        "Opening.", "ГЛАВА " + repeated("Ж", 74), "Text.", "ГЛАВА " + repeated("Ж", 75), "e\u0301pilogue", "\u212Aapitel 5",
    }, "\n");

    BookDraft book = TextBook::read(gutenberg, "fallback");
    check("text: a Gutenberg file names the book and keeps only what lies between its markers",
        book.title == "A Small Book" && book.author == "Jane Doe" && book.language.empty()
            && draftOutline(book) == "A Small Book = [Illustration] || CHAPTER I. Down the Hole = #CHAPTER I. Down the Hole / "
                "Alice was beginning to get very tired of sitting. || CHAPTER II. Up the Hill = #CHAPTER II. Up the Hill / So she was considering.",
        book.title + " / " + book.author + " / " + draftOutline(book));

    check("text: wrapped lines joined; numbers and standard upper-case roman numerals open chapters; a run of headings is titled by its last two",
        draftOutline(TextBook::read(numbered, "Numbers")) == "Numbers = A preface that is wrapped across two lines. || I = #I / The first. "
            "|| XIV — 12 = #II. / #XIV / #12 / Twelve. / iv / 1.5 / DID / 1234 || MMMCMXCIX. = #MMMCMXCIX.",
        draftOutline(TextBook::read(numbered, "Numbers")));

    check("text: one paragraph per line without blank lines; a chapter word alone, before a number or in capitals",
        draftOutline(TextBook::read(lines, "Lines")) == "Lines = A first line || Chapter 1 = #Chapter 1 / It was a dark night. "
            "|| CHAPTER TWO: THE STORM — Chapter = #CHAPTER TWO: THE STORM / #Chapter / The wind howled. / Chapters are not headings. / Chapter: one",
        draftOutline(TextBook::read(lines, "Lines")));

    check("text: Russian and Ukrainian headings, a part's joined to its first chapter's, CR line ends",
        draftOutline(TextBook::read(russian, "Ru")) == "Ru = Предисловие. || ЧАСТЬ ПЕРВАЯ — I = #ЧАСТЬ ПЕРВАЯ / #I / Мой дядя. "
            "|| II = #II / Книга лежала на столе. / Глава первая || ЧАСТЬ ВТОРАЯ — I = #ЧАСТЬ ВТОРАЯ / #I / Он уехал. "
            "|| Пролог = #Пролог / Пришла весна. || Розділ 3 = #Розділ 3 / Кінець.",
        draftOutline(TextBook::read(russian, "Ru")));

    check("text: French headings; no-break spaces kept, a line of them blank",
        draftOutline(TextBook::read(french, "Fr")) == "CHAPITRE PREMIER = #CHAPITRE PREMIER / Il dit\u00a0: «\u00a0Bonjour\u00a0» / Première partie "
            "|| ÉPILOGUE = #ÉPILOGUE / \u00a0Fin\u00a0",
        draftOutline(TextBook::read(french, "Fr")));

    check("text: a heading is at most 80 code points, its word spelled alike",
        draftOutline(TextBook::read(lengths, "Len")) == "Len = Opening. || ГЛАВА " + repeated("Ж", 74) + " = #ГЛАВА " + repeated("Ж", 74)
            + " / Text. / ГЛАВА " + repeated("Ж", 75) + " / e\u0301pilogue || \u212Aapitel 5 = #\u212Aapitel 5",
        draftOutline(TextBook::read(lengths, "Len")));

    std::vector<std::string> wrong;
    for (const auto& text : opening) if (!TextBook::opensChapter(text)) wrong.push_back(text);
    for (const auto& text : notOpening) if (TextBook::opensChapter(text)) wrong.push_back(text);
    check("text: what opens a chapter and what does not; a no-break space parts words", wrong.empty(), Text::join(wrong, " | "));

    check("text: a list of contents running into the book is titled by its last two headings",
        draftOutline(TextBook::read(parts, "Parts")) == "Parts = Contents || BOOK ONE: 1805 — CHAPTER I = #BOOK ONE: 1805 / #CHAPTER I / #CHAPTER II "
            "/ #BOOK TWO: 1805 / #CHAPTER I / #BOOK ONE: 1805 / #CHAPTER I / Well, Prince. || CHAPTER II = #CHAPTER II / Anna Pavlovna. "
            "|| BOOK TWO: 1805 — CHAPTER I = #BOOK TWO: 1805 / #CHAPTER I / In October.",
        draftOutline(TextBook::read(parts, "Parts")));

    auto plain = [](int count) {
        std::vector<std::string> paragraphs;
        for (int i = 1; i <= count; ++i) paragraphs.push_back("Paragraph " + std::to_string(i) + ".");
        return Text::join(paragraphs, "\n\n");
    };
    auto sizes = [](const BookDraft& draft) {
        std::string out;
        for (const auto& chapter : draft.chapters) out += (out.empty() ? "" : " ") + chapter.title + ":" + std::to_string(chapter.paragraphs.size());
        return out;
    };
    BookDraft cut = TextBook::read(plain(250), "Parts");
    BookDraft one = TextBook::read(plain(100), "One");
    check("text: without headings, every 100 paragraphs make a part; 100 or fewer are one chapter named by the book",
        sizes(cut) == "Part 1:100 Part 2:100 Part 3:50" && cut.chapters[1].paragraphs[0].text == "Paragraph 101."
            && one.chapters.size() == 1 && one.chapters[0].title == "One",
        sizes(cut));

    BookDraft empty = TextBook::read("", "Empty");
    BookDraft startOnly = TextBook::read("Title: Not Taken\n*** START OF SOMETHING ***\nBody.", "Start");
    check("text: an empty file has no chapters; a start marker without an end is not Gutenberg's",
        empty.title == "Empty" && empty.author.empty() && empty.chapters.empty()
            && startOnly.title == "Start" && draftOutline(startOnly) == "Start = Title: Not Taken / *** START OF SOMETHING *** / Body.",
        draftOutline(startOnly));
}

/// The HTML reader: the same cases as the web app's
/// `tools/checks/HtmlBook.mjs`, since the apps must read an HTML file alike.
static void checkHtmlBook() {
    std::string nested = R"(<!DOCTYPE html>
<html lang=" FR-fr "><head><meta charset="utf-8">
<meta name="Author" content="  Antoine   de Saint-Exupéry "><meta name="author" content="Second">
<title>  Le   Petit
  Prince </title></head>
<body><div>Loose text
  <div><h1>Le Petit <em>Prince</em></h1><h3>Une histoire</h3>
    <div>Lorsque j’avais <i>six</i> ans
      <div>Nested <span>inner</span> div.</div>
      After it.</div>
    <p>Il dit&nbsp;: «&nbsp;Dessine-moi un mouton&nbsp;»</p></div>
  <section><h2>Chapitre II</h2><article><p>J’ai vécu seul.</p></article></section>
</div></body></html>)";

    std::string scripts = R"(<html><head><title>Scripts</title>
<style>a < b { }</style><script>if (a < b) { x = "</div>"; }</script></head><body>
<p>Before.</p><script src="x.js"/><p>After a closed script.</p>
<SCRIPT>var s = "<h1>Not a heading</h1>";</SCRIPT><p>After an upper-case one.</p>
<!-- <script> in a comment --><p>After the comment.</p>
<noscript><p>No.</p></noscript><template><p>No.</p></template>
<svg><title>No</title><svg><text>No</text></svg><text>No</text></svg><math><mi>x</mi></math>
<nav><ul><li>No</li><li><nav>No</nav></li><li>No</li></ul></nav><p>Last.</p></body></html>)";

    std::string blocks = R"(<body><h2>Contents</h2><table><caption>Chapters</caption>
<tr><th>Number</th><th>Title</th></tr><tr><td>I.</td><td>Down the <a href="#1">Hole</a></td></tr></table>
<ul><li>One</li><li>Two<ul><li>Inner</li></ul></li></ul><dl><dt>Term</dt><dd>Meaning</dd></dl>
<pre>
  Some   preformatted
     text
</pre>text<hr>after a rule</body>)";

    std::string breaks = R"(<html><head><title>Breaks</title></head><body>
<h2><a id="c1"></a>CHAPTER I.<br>
Down the Rabbit-Hole</h2><p>Line one<br/>Line two<BR>&nbsp;<br />Line three</p>
<h3>A minor<br>heading</h3><p>Text<br></br>after an end br.</p></body></html>)";

    std::string broken = R"(<?xml version="1.0"?><!DOCTYPE html>
<P>An unclosed paragraph
<P>Entities: &amp; &eacute;t&eacute; &#233;&#x2014;&unknown; &lt;tag&gt;
</div></span></p><H2>Upper Case</H2>
<p>Stray <b>bold</i> end</em> tags.
<p>A <![CDATA[<b>cdata</b>]]> and a <!-- comment --> inside.
<div>Unclosed div
<p>No end tags at all)";

    std::string gutenberg = R"(<html lang="en"><head><title>A Small Book | Project Gutenberg</title></head><body>
<header><h2>The Project Gutenberg eBook of A Small Book</h2>
<p><strong>Title</strong>: A Small Book</p><p><strong>Author</strong>: Jane Doe</p>
<div>*** START OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***</div></header>
<h1>A Small Book</h1><h2>by Jane Doe</h2>
<div class="chapter"><h2>CHAPTER I.<br>Down the Hole</h2><p>Alice was tired.</p></div>
<div class="chapter"><h2>CHAPTER II.</h2><p>Up the hill.</p></div>
<footer><div>*** END OF THE PROJECT GUTENBERG EBOOK A SMALL BOOK ***</div>
<h2>THE FULL PROJECT GUTENBERG LICENSE</h2><p>The license.</p></footer></body></html>)";

    std::string untitled = R"(<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="RU"><head><title> </title><meta name="author" content=""/></head>
<body><h1>Мастер<br/>и Маргарита</h1><p>&nbsp;</p><html:p>Однажды весною.</html:p><h2>Глава 1</h2><p>Никогда.</p></body></html>)";

    // A head never closed and no body: an element a head cannot hold begins the text.
    std::string headless = R"(<html><head><title>Kafka</title><meta name="author" content="Franz Kafka"><link rel="stylesheet" href="a.css"><base href="/">
<noscript>No script.</noscript>Text in the head is not the book's.
<span>Erster Teil</span> begins the text, as in a browser.<h1>ERSTER TEIL</h1><h2>Kapitel 1</h2><p>Als Gregor Samsa erwachte.</p>)";

    BookDraft book = HtmlBook::read(nested, "nested");
    check("html: the title, the first author, the language lowercased; nested blocks each end a paragraph",
        book.title == "Le Petit Prince" && book.author == "Antoine de Saint-Exupéry" && book.language == "fr-fr"
            && draftOutline(book) == "Le Petit Prince = Loose text || Le Petit Prince = #Le Petit Prince / #Une histoire / Lorsque j’avais six ans "
                "/ Nested inner div. / After it. / Il dit\u00a0: «\u00a0Dessine-moi un mouton\u00a0» || Chapitre II = #Chapitre II / J’ai vécu seul.",
        book.title + " / " + book.author + " / " + book.language + " / " + draftOutline(book));

    check("html: scripts, styles and other hidden elements say nothing, whatever they hold",
        draftOutline(HtmlBook::read(scripts, "scripts")) == "Scripts = Before. / After a closed script. / After an upper-case one. / After the comment. / Last.",
        draftOutline(HtmlBook::read(scripts, "scripts")));

    check("html: table cells, list items, definitions, preformatted text and a rule each end a paragraph",
        draftOutline(HtmlBook::read(blocks, "blocks")) == "Contents = #Contents / Chapters / Number / Title / I. / Down the Hole / One / Two / Inner "
            "/ Term / Meaning / Some preformatted text / text / after a rule",
        draftOutline(HtmlBook::read(blocks, "blocks")));

    check("html: a break ends a paragraph, but in a heading it is a space",
        draftOutline(HtmlBook::read(breaks, "breaks")) == "CHAPTER I. Down the Rabbit-Hole = #CHAPTER I. Down the Rabbit-Hole / Line one / Line two "
            "/ Line three / #A minor heading / Text / after an end br.",
        draftOutline(HtmlBook::read(breaks, "breaks")));

    check("html: unclosed paragraphs, stray end tags, entities, CDATA, comments, upper case, no html or body",
        draftOutline(HtmlBook::read(broken, "broken")) == "broken = An unclosed paragraph / Entities: & été é—&unknown; <tag> "
            "|| Upper Case = #Upper Case / Stray bold end tags. / A <b>cdata</b> and a inside. / Unclosed div / No end tags at all",
        draftOutline(HtmlBook::read(broken, "broken")));

    BookDraft pg = HtmlBook::read(gutenberg, "pg");
    check("html: a Gutenberg book is named by its header and keeps only what lies between its markers; a run of h1 and h2 opens one chapter, titled by its last two",
        pg.title == "A Small Book" && pg.author == "Jane Doe" && pg.language == "en"
            && draftOutline(pg) == "by Jane Doe — CHAPTER I. Down the Hole = #A Small Book / #by Jane Doe / #CHAPTER I. Down the Hole / Alice was tired. "
                "|| CHAPTER II. = #CHAPTER II. / Up the hill.",
        pg.title + " / " + pg.author + " / " + draftOutline(pg));

    BookDraft first = HtmlBook::read(untitled, "untitled");
    check("html: without a title the first h1 names the book; xml:lang; a no-break space alone is no paragraph",
        first.title == "Мастер и Маргарита" && first.author.empty() && first.language == "ru"
            && draftOutline(first) == "Мастер и Маргарита = #Мастер и Маргарита / Однажды весною. || Глава 1 = #Глава 1 / Никогда.",
        first.title + " / " + first.language + " / " + draftOutline(first));

    BookDraft kafka = HtmlBook::read(headless, "headless");
    check("html: an element that cannot be in the head ends it, closed or not; an h1 and an h2 together title their chapter",
        kafka.title == "Kafka" && kafka.author == "Franz Kafka"
            && draftOutline(kafka) == "Kafka = Erster Teil begins the text, as in a browser. || ERSTER TEIL — Kapitel 1 = #ERSTER TEIL / #Kapitel 1 / Als Gregor Samsa erwachte.",
        kafka.title + " / " + kafka.author + " / " + draftOutline(kafka));

    std::string paragraphs;
    for (int i = 1; i <= 130; ++i) paragraphs += "<p>Paragraph " + std::to_string(i) + ".";
    BookDraft many = HtmlBook::read(paragraphs + "<h3>Minor</h3>", "many");
    BookDraft empty = HtmlBook::read("", "empty");
    std::string sizes;
    for (const auto& chapter : many.chapters) sizes += (sizes.empty() ? "" : " ") + chapter.title + ":" + std::to_string(chapter.paragraphs.size());
    check("html: without h1 or h2, every 100 paragraphs make a part; an empty file has no chapters",
        many.title == "many" && sizes == "Part 1:100 Part 2:31"
            && empty.title == "empty" && empty.author.empty() && empty.language.empty() && empty.chapters.empty(),
        sizes);
}

/// The FB2 reader: the same cases as the web app's `tools/checks/Fb2.mjs`,
/// since the apps must read a FictionBook alike.
static void checkFb2() {
    std::string novel = R"fb2(<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
<description>
<title-info><author><first-name>Lev</first-name><middle-name></middle-name><last-name>Tolstoy</last-name><nickname>LNT</nickname></author>
<author><first-name>Second</first-name></author><book-title>War and
  Peace</book-title><annotation><p>Not read.</p></annotation><lang>RU</lang></title-info>
<src-title-info><book-title>Война и мир</book-title><lang>ru</lang></src-title-info>
<document-info><author><nickname>scanner</nickname></author></document-info>
</description>
<body>
<title><p>Leo Tolstoy</p></title>
<epigraph><p>An epigraph.</p></epigraph>
<section><title><p>Part One</p></title>
<section><title><p>Chapter 1</p></title><p>Well, <emphasis>Prince</emphasis>.<a l:href="#n1" type="note">[1]</a></p><empty-line/><image l:href="#pic"/></section>
<section><p>An untitled section.</p></section>
</section>
</body>
<body name="notes"><title><p>Notes</p></title><section id="n1"><title><p>1</p></title><p>A note.</p></section></body>
<binary id="pic" content-type="image/png">iVBORw0KGgo=</binary>
</FictionBook>)fb2";
    BookDraft book = Fb2Book::read(novel, "novel");
    check("fb2 reads title-info: the first author, the title, the language lowercased",
          book.title == "War and Peace" && book.author == "Lev Tolstoy" && book.language == "ru",
          book.title + " / " + book.author + " / " + book.language);
    check("fb2 chapters: the opening before the sections, a part with its first chapter titled by both, an untitled section, the notes",
          draftOutline(book) == "War and Peace = #Leo Tolstoy / An epigraph. || Part One — Chapter 1 = #Part One / #Chapter 1 / Well, Prince.[1] "
                              "|| War and Peace = An untitled section. || Notes = #Notes / #1 / A note.",
          draftOutline(book));

    std::string verses = R"fb2(<FictionBook><description><title-info><author><nickname>Anon</nickname></author><book-title>Verses</book-title></title-info></description>
<body><section><title><p>Poems</p></title>
<poem><title><p>A Song</p></title><stanza><v>First line,</v><v>second line.</v></stanza><text-author>Anon</text-author><date>1805</date></poem>
<subtitle>* * *</subtitle>
<table><tr><th>Name</th><th>Age</th></tr><tr><td>Pierre</td><td></td><td>20</td></tr></table>
)fb2" "<p>Il dit\xC2\xA0: «\xC2\xA0" "Bonjour\xC2\xA0»\t et\r\n  part.</p>" R"fb2(
</section></body>
<body><section><p>A body with no title.</p></section></body></FictionBook>)fb2";
    BookDraft poems = Fb2Book::read(verses, "verses");
    check("fb2 verse lines, subtitles and table rows, a no-break space kept, an untitled later body as Notes",
          poems.author == "Anon" && poems.language.empty()
              && draftOutline(poems) == "Poems = #Poems / #A Song / First line, / second line. / Anon / 1805 / #* * * / Name | Age / Pierre | 20 "
                                      "/ Il dit\xC2\xA0: «\xC2\xA0" "Bonjour\xC2\xA0» et part. || Notes = A body with no title.",
          draftOutline(poems));

    // A notes body before the text, which is the first body without a name;
    // parts whose chapters restart at I; spacer paragraphs of no-break spaces.
    std::string volumes = R"fb2(<FictionBook><description><title-info><book-title>Война и мир</book-title></title-info></description>
<body name="notes"><title><p>Примечания</p></title><section><p>Сноска.</p></section></body>
<body><title><p>Лев Толстой</p></title>
<section><title><p>ТОМ ПЕРВЫЙ</p></title>
<section><title><p>ЧАСТЬ ПЕРВАЯ</p></title>
<section><title><p>I</p></title><p>&#160;</p><p>— Eh bien, mon prince.</p><p> &#160; </p></section>
<section><title><p>II</p></title><table><tr><td>&#160;</td><td>Да</td></tr><tr><td>&#160;</td></tr></table></section>
</section>
<section><title><p>ЧАСТЬ ВТОРАЯ</p></title>
<section><title><p>I</p></title><p>В октябре.</p></section>
</section></section>
<section><section><title><p>&#160;</p><p>ЭПИЛОГ</p></title><p>Прошло семь лет.</p></section></section>
</body>
<body name="comments"><section><p>Комментарий.</p></section></body>
<body><p>A second body without a name.</p></body></FictionBook>)fb2";
    check("fb2: the text is the first body without a name, the others follow in file order; a part's title joins its first chapter's; paragraphs of no-break spaces dropped",
          draftOutline(Fb2Book::read(volumes, "volumes")) == "ТОМ ПЕРВЫЙ — ЧАСТЬ ПЕРВАЯ — I = #Лев Толстой / #ТОМ ПЕРВЫЙ / #ЧАСТЬ ПЕРВАЯ / #I / — Eh bien, mon prince. "
                                                             "|| II = #II / Да || ЧАСТЬ ВТОРАЯ — I = #ЧАСТЬ ВТОРАЯ / #I / В октябре. || ЭПИЛОГ = #ЭПИЛОГ / Прошло семь лет. "
                                                             "|| Примечания = #Примечания / Сноска. || Notes = Комментарий. || Notes = A second body without a name.",
          draftOutline(Fb2Book::read(volumes, "volumes")));

    BookDraft named = Fb2Book::read(R"fb2(<FictionBook><body name="main"><section><title><p>Part One</p></title><section><title><p>Chapter 1</p></title>
<p>Text.</p></section></section></body><body name="notes"><section><p>A note.</p></section></body></FictionBook>)fb2", "named");
    check("fb2: when every body has a name, the first is the text",
          draftOutline(named) == "Part One — Chapter 1 = #Part One / #Chapter 1 / Text. || Notes = A note.", draftOutline(named));

    std::string broken = R"fb2(<FictionBook><description><title-info><book-title>Broken</book-title></title-info></description>
<body><section><title>Loose title</title><p>Unclosed<p>Stray</emphasis> end.</p><empty-line><p>After &amp; <![CDATA[<raw>]]></p>
<section><title><p>Cut</p></title><p>Short)fb2";
    check("fb2 tolerates unclosed and stray tags and a file cut short",
          draftOutline(Fb2Book::read(broken, "broken")) == "Loose title = #Loose title / Unclosed / Stray end. / After & <raw> || Cut = #Cut / Short",
          draftOutline(Fb2Book::read(broken, "broken")));

    BookDraft bodiless = Fb2Book::read(R"fb2(<FictionBook><description><title-info><author><last-name>Nobody</last-name></author>
<book-title>No Body</book-title></title-info></description></FictionBook>)fb2", "bodiless");
    BookDraft empty = Fb2Book::read("", "fallback");
    check("fb2 without a body has no chapters; an empty file takes the fallback title",
          bodiless.title == "No Body" && bodiless.author == "Nobody" && bodiless.chapters.empty()
              && empty.title == "fallback" && empty.author.empty() && empty.language.empty() && empty.chapters.empty());

    BookDraft plain = Fb2Book::read("<FictionBook><body><p>One.</p><p>Two.</p></body></FictionBook>", "plain");
    check("fb2 without sections is one chapter titled by the book", draftOutline(plain) == "plain = One. / Two.", draftOutline(plain));
}

/// How a book file's bytes become text: the same cases as the web app's
/// check (`tools/checks/Charset.mjs`), since the apps must read a file alike.
static void checkCharset() {
    auto read = [](const std::string& bytes) { return Charset::decode(bytes, Charset::declared(bytes)); };
    auto utf16 = [](const std::string& utf8, bool bigEndian) {
        glong count = 0;
        gunichar2* units = g_utf8_to_utf16(utf8.c_str(), -1, nullptr, &count, nullptr);
        std::string out;
        for (glong i = 0; i < count; ++i) {
            char high = static_cast<char>(units[i] >> 8), low = static_cast<char>(units[i] & 0xFF);
            out += bigEndian ? std::string{high, low} : std::string{low, high};
        }
        g_free(units);
        return out;
    };
    std::string bom = "\xEF\xBB\xBF";
    std::string badByte = read(bom + "Chapter 1\n\nIt\x92" "s fine.");
    check("charset: after a UTF-8 mark, a bad byte is one U+FFFD", badByte == "Chapter 1\n\nIt�" "s fine.", badByte);
    std::string subparts = read(bom + "a\xF0\x9F\x98" "b\xED\xA0\x80" "c\xC0\xAF" "d\xF4\x90\x80\x80" "e\xE2\x82");
    check("charset: bad UTF-8 replaced by maximal subparts, as TextDecoder does",
          subparts == "a�" "b���" "c��" "d����" "e�", subparts);
    std::string doubled = read(bom + bom + "Text") + " " + read("\xFF\xFE\xFF\xFE" + utf16("Text", false));
    check("charset: a doubled byte-order mark is dropped", doubled == "Text Text", doubled);
    std::string marked = read("\xFF\xFE" + utf16("A", false) + "\x3D\xD8" + utf16("B😀", false) + "A") + " " + read("\xFE\xFF" + utf16("Hi", true));
    check("charset: UTF-16 after its mark, a pair joined, a lone surrogate and an odd last byte each one U+FFFD",
          marked == "A�" "B😀� Hi", marked);
    std::string unmarked = read(utf16("Chapter 1\n\nText.", false)) + " | " + read(utf16("Глава 1", true));
    check("charset: UTF-16 without a mark, told by its zero bytes, either way round", unmarked == "Chapter 1\n\nText. | Глава 1", unmarked);
    std::string sentence = "Это был тихий вечер в Москве.";
    std::string nul = read(sentence + std::string(1, '\0') + sentence);
    check("charset: a stray NUL does not make UTF-8 into UTF-16, and is dropped", nul == sentence + sentence, nul);

    std::string wrong = R"(<?xml version="1.0" encoding="windows-1251"?><p>Война</p>)";
    std::string stray = read("Глава 1\n\nТихий вечер \x97 в Москве.");
    check("charset: valid UTF-8 wins over a wrong declaration, and keeps its text past a stray byte",
          read(wrong) == wrong && stray == "Глава 1\n\nТихий вечер � в Москве.", stray);
    std::string ascii = read("It\x92" "s");
    check("charset: ASCII with one stray byte is windows-1252, not a U+FFFD", ascii == "It’s", ascii);
    std::string declared = read(R"(<?xml encoding="windows-1251"?>)" "\xC2\xEE\xE9\xED\xE0") + " | "
        + read(R"(<?xml encoding="koi8-r"?>)" "\xE7\xCC\xC1\xD7\xC1") + " | " + read(R"(<meta charset="iso-8859-1">)" "\x80 Tr\xE4" "ume");
    check("charset: a declared windows-1251, koi8-r or windows-1252",
          declared == R"(<?xml encoding="windows-1251"?>Война | <?xml encoding="koi8-r"?>Глава | <meta charset="iso-8859-1">€ Träume)", declared);
    std::string base64;
    for (int i = 0; i < 20; ++i) base64 += "QUJDREVGR0hJSktM";
    std::string cover = read("<FictionBook><book-title>\xC2\xEE\xE9\xED\xE0 \xE8 \xEC\xE8\xF0</book-title><binary>" + base64 + "</binary></FictionBook>");
    std::string latinHeavy = read("Chapter 1\n\nThe word \xAB\xE2\xE5\xF7\xE5\xF0\xBB means evening in Russian.");
    check("charset: nothing declared, runs of high bytes are windows-1251, markup and base64 aside",
          Text::contains(cover, "<book-title>Война и мир</book-title>") && latinHeavy == "Chapter 1\n\nThe word «вечер» means evening in Russian.",
          latinHeavy);
    std::string western = read("Tr\xE4" "ume und Caf\xE9");
    check("charset: nothing declared, accents inside Latin words are windows-1252", western == "Träume und Café", western);
    std::string controls = read("a\x0C" "b\x0B" "c\x01" "d\x1A" "e\x7F" "f\tg\r\n\x1F");
    check("charset: a form feed or vertical tab ends a line; the other controls XML forbids are dropped", controls == "a\nb\ncdef\tg\r\n", controls);
    std::string names = Charset::declared("<?XML version=\"1.0\" ENCODING = \" Windows-1251\n\"?>") + "|" + Charset::declared("<META CHARSET=KOI8-R>")
        + "|" + Charset::declared(R"(<meta http-equiv="Content-Type" content="text/html; charset=windows-1252">)") + "|" + Charset::declared("<p>none</p>");
    check("charset: a declared name lowercased and trimmed of spaces and line breaks; none is empty", names == "windows-1251|koi8-r|windows-1252|", names);
}

/// The markup scanner, event for event: the same cases as the web app's check.
static void checkScanner() {
    // `<name key=value>`, `</name>` and `"text"`, on one line.
    auto events = [](const std::string& markup) {
        std::vector<std::string> out;
        XmlScanner::scan(markup, {
            [&](const std::string& name, const XmlScanner::Attributes& attributes) {
                std::string tag = "<" + name;
                for (const auto& [key, value] : attributes) tag += " " + key + "=" + value;
                out.push_back(tag + ">");
            },
            [&](const std::string& name) { out.push_back("</" + name + ">"); },
            [&](const std::string& text) { out.push_back("\"" + text + "\""); },
        });
        return Text::join(out, " ");
    };
    std::string lt = events("<p>if 1 < 2 then x</p><p>y</p>x<");
    check("scanner: a '<' that opens no tag is text", lt == R"(<p> "if 1 < 2 then x" </p> <p> "y" </p> "x<")", lt);
    check("scanner: an empty end tag is skipped", events("<p>a</>b</p>") == R"(<p> "a" "b" </p>)", events("<p>a</>b</p>"));
    std::string names = events("<DIV CLASS=X Xml:Lang='TR'><Iİ>t</Iİ><ns:P/></DIV><İ>");
    check("scanner: names lowercased by ASCII alone, without their namespace; '<' before a letter not ASCII is text",
          names == R"(<div class=X lang=TR> <iİ> "t" </iİ> <p> </p> </div> "<İ>")", names);
    std::string references = XmlScanner::decodeEntities("&amp;&lt;&#65;&#x42;&#X43;&#0065;&#-1;&#+65;&# 65;&#x0x41;&#65&#66;&#xD83D;&#x110000;&#0;"
        "&unknown;&abcdefghij;&#x1F600;&shy;&nbsp;");
    check("scanner: references are '&', 1-9 letters, digits or '#', ';'; a number not a scalar value is dropped",
          references == "&<ABCA&#-1;&#+65;&# 65;&#65B&unknown;&abcdefghij;😀 ", references);

    // GTK and Qt take the locale from the environment; under a Turkish one
    // GLib alone would lower 'I' to a dotless 'ı'.
    std::string previous = setlocale(LC_ALL, nullptr);
    if (!setlocale(LC_ALL, "tr_TR.UTF-8")) {
        std::printf("skip  lowercasing under a Turkish locale (tr_TR.UTF-8 is not installed)\n");
        return;
    }
    std::string lowered = Text::lower("TITLE IŞIK İSTANBUL");
    check("lowercasing under a Turkish locale: 'I' is 'i' and 'İ' an 'i' with a dot above, as in the other apps",
          lowered == "title işik i̇stanbul" && events("<TITLE>") == "<title>", lowered);
    setlocale(LC_ALL, previous.c_str());
}

/// Cyrillic text, as UTF-8, in windows-1251, where А–я are 0xC0–0xFF.
static std::string windows1251(const std::string& text) {
    std::string out;
    for (const char* p = text.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        out += static_cast<char>(c < 0x80 ? c : c - 0x410 + 0xC0);
    }
    return out;
}

/// The importer as the library runs it: a file of each format made into an
/// EPUB in the books folder, and read back as a book.
static void checkBookImporter(const std::string& folder) {
    std::string picked = Files::join(folder, "picked");
    std::string books = Files::join(folder, "books");
    Files::ensureDirectory(picked);
    Files::ensureDirectory(books);
    auto convert = [&](const std::string& name, const std::string& contents, std::string* error) {
        Files::write(Files::join(picked, name), contents);
        return BookImporter::convert(Files::join(picked, name), books, error);
    };
    // The shelf's metadata, the contents, and the first chapter's text.
    auto summary = [](const std::string& epub) {
        std::string error;
        auto metadata = EpubLoader::metadata(epub, &error);
        auto book = EpubLoader::load(epub, &error, false);
        if (!metadata || !book) return error;
        std::vector<std::string> titles;
        for (const auto& entry : book->contents) titles.push_back(entry.title);
        return metadata->title + " / " + metadata->author + " / " + metadata->language + " / " + Text::join(titles, " | ")
            + " / " + Text::replaceAll(book->chapters[0].text, "\n", " ¶ ");
    };

    check("the importer knows a book by its name's ending, in any case",
          BookImporter::reads("a/War.FB2.ZIP") && BookImporter::reads("b.fbz") && BookImporter::reads("c.zip") && BookImporter::reads("d.Txt")
              && BookImporter::reads("e.xhtml") && BookImporter::reads("f.HTM") && !BookImporter::reads("g.epub")
              && !BookImporter::reads("h.pdf") && !BookImporter::reads("i.docx") && !BookImporter::reads("fb2"));
    check("a converted book is named by FNV-1a 64 of its file, as in the other apps",
          EpubWriter::identifier("pdf", "hello") == "urn:aireader:pdf:a430d84680aabd0b"
              && EpubWriter::identifier("txt", "") == "urn:aireader:txt:cbf29ce484222325",
          EpubWriter::identifier("pdf", "hello"));

    std::string error;
    std::string fb2 = convert("tolstoy.fb2", windows1251(R"(<?xml version="1.0" encoding="windows-1251"?>
<FictionBook><description><title-info><author><first-name>Лев</first-name><last-name>Толстой</last-name></author>
<book-title>Война и мир</book-title><lang>ru</lang></title-info></description>
<body><section><title><p>Глава первая</p></title><p>Ну что, князь.</p></section>
<section><title><p>Глава вторая</p></title><p>Анна Павловна кашляла.</p></section></body></FictionBook>)"), &error);
    check("an fb2 in windows-1251 becomes an epub named after the file, its language the book's own",
          Text::endsWith(fb2, "/tolstoy.epub")
              && summary(fb2) == "Война и мир / Лев Толстой / ru / Глава первая | Глава вторая / Глава первая ¶ Ну что, князь.",
          error.empty() ? summary(fb2) : error);
    std::string tolstoy = *Files::read(Files::join(picked, "tolstoy.fb2"));
    std::string again = convert("tolstoy.fb2", tolstoy, &error);
    std::string other = convert("tolstoy.fb2", "<FictionBook><body><p>Другая книга.</p></body></FictionBook>", &error);
    std::string otherAgain = convert("tolstoy.fb2", "<FictionBook><body><p>Другая книга.</p></body></FictionBook>", &error);
    check("the same file converted again finds the epub made before; another of its name gets a free one",
          again == fb2 && !fb2.empty() && Text::endsWith(other, "/tolstoy (2).epub") && otherAgain == other
              && Text::startsWith(summary(other), "tolstoy /  / und"),
          again + " | " + other + " | " + otherAgain);

    // The resource forks a Mac adds, which end .fb2 too, come first here, and
    // a second entry of the book's name after it.
    ZipWriter zipped;
    zipped.add("__MACOSX/._book.FB2", std::string("\0\5\26\7", 4));
    zipped.add("._book.FB2", std::string("\0\5\26\7", 4));
    zipped.add("readme.txt", "Read me.");
    zipped.add("book.FB2", R"(<FictionBook><description><title-info><author><first-name>Иван</first-name><last-name>Тургенев</last-name></author>
<book-title>Отцы и дети</book-title></title-info></description><body><section><title><p>I</p></title><p>— Что, Петр, не видать еще?</p></section></body></FictionBook>)");
    zipped.add("book.FB2", "<FictionBook><body><p>Не эта.</p></body></FictionBook>");
    std::string zip = zipped.finished();
    std::string fbz = convert("Kniga.fb2.zip", zip, &error);
    auto archive = ZipArchive::open(fbz);
    std::string package = archive ? archive->contents("OEBPS/content.opf").value_or("") : "";
    check("a zipped fb2: the first fb2 the archive lists, not a Mac's resource fork, named by the zip's bytes; no language read is und",
          Text::endsWith(fbz, "/Kniga.epub") && summary(fbz) == "Отцы и дети / Иван Тургенев / und / I / I ¶ — Что, Петр, не видать еще?"
              && Text::contains(package, ">" + EpubWriter::identifier("fb2", zip) + "<"),
          error.empty() ? summary(fbz) : error);

    std::string txt = convert("Le Petit Prince.TXT", "CHAPITRE PREMIER\r\n\r\nLorsque j'avais six ans j'ai vu, une fois, une magnifique image, "
        "dans un livre sur la forêt vierge qui s'appelait « Histoires vécues ». Ça représentait un serpent boa qui avalait un fauve.\r\n\r\n"
        "CHAPITRE II\r\n\r\nJ'ai ainsi vécu seul, sans personne avec qui parler véritablement, jusqu'à une panne dans le désert du Sahara, "
        "il y a six ans.\r\n", &error);
    check("a text file: titled by its name, its language read off the prose",
          Text::endsWith(txt, "/Le Petit Prince.epub") && summary(txt) == "Le Petit Prince /  / fr / CHAPITRE PREMIER | CHAPITRE II / "
              "CHAPITRE PREMIER ¶ Lorsque j'avais six ans j'ai vu, une fois, une magnifique image, dans un livre sur la forêt vierge "
              "qui s'appelait « Histoires vécues ». Ça représentait un serpent boa qui avalait un fauve.",
          error.empty() ? summary(txt) : error);

    std::string html = convert("kafka.htm", "<html lang=\"de\"><head><meta charset=\"windows-1252\"><title>Die Verwandlung</title>"
        "<meta name=\"author\" content=\"Franz Kafka\"></head><body><h1>Erstes Kapitel</h1>"
        "<p>Als Gregor Samsa eines Morgens aus unruhigen Tr\xE4umen erwachte.</p>"
        "<h2>Zweites Kapitel</h2><p>Erst in der Abendd\xE4mmerung erwachte Gregor.</p></body></html>", &error);
    check("an html page in the encoding it names",
          Text::endsWith(html, "/kafka.epub") && summary(html) == "Die Verwandlung / Franz Kafka / de / Erstes Kapitel | Zweites Kapitel / "
              "Erstes Kapitel ¶ Als Gregor Samsa eines Morgens aus unruhigen Träumen erwachte.",
          error.empty() ? summary(html) : error);

    // An EPUB renamed .zip is shelved as the EPUB it is.
    EpubBuilder::Metadata petit;
    petit.title = "Le Petit Prince";
    petit.language = "fr";
    petit.identifier = "urn:uuid:petit";
    std::string epubBytes = EpubWriter::epub(petit, {ChapterDraft{"I", {{"Lorsque j'avais six ans.", false}}}});
    std::string renamed = convert("Petit.ZIP", epubBytes, &error);
    check("a zip holding an epub, not an fb2, is that epub, as it is",
          Text::endsWith(renamed, "/Petit.epub") && Files::read(renamed) == epubBytes && BookImporter::epub(epubBytes, "Petit.ZIP") == epubBytes
              && convert("Petit.ZIP", epubBytes, &error) == renamed && EpubWriter::identifierOf(renamed) == "urn:uuid:petit",
          error.empty() ? renamed : error);

    ZipWriter notes;
    notes.add("notes.txt", "Not a book.");
    // A method this reader does not inflate (12 is bzip2), in the local
    // header and the central directory both.
    ZipWriter bzipped;
    bzipped.add("book.fb2", "<FictionBook><body><p>Text.</p></body></FictionBook>", false);
    std::string bz = bzipped.finished();
    bz[8] = 12;
    bz[bz.find("PK\x01\x02") + 10] = 12;
    std::string noFb2, notZip, noText, unreadable;
    convert("notes.zip", notes.finished(), &noFb2);
    convert("broken.fbz", "not a zip", &notZip);
    convert("empty.txt", "\n \t\n", &noText);
    convert("bz.fb2.zip", bz, &unreadable);
    check("refused, writing nothing: a zip with no fb2, a file that is no zip, a file with no text, an fb2 that cannot be unpacked",
          noFb2 == "The ZIP holds no FB2 book." && notZip == "The file is not a ZIP archive." && noText == "The file has no text to read."
              && unreadable == "The FB2 book in the ZIP cannot be read." && !Files::exists(Files::join(books, "notes.epub"))
              && !Files::exists(Files::join(books, "empty.epub")) && !Files::exists(Files::join(books, "bz.epub")),
          noFb2 + " | " + notZip + " | " + noText + " | " + unreadable);
}

static void checkWordContext() {
    std::string text = "Les maisons étaient vieilles. Il pleuvait fort.\nNouveau paragraphe ici.";
    WordContext context(text, "fr");
    auto selection = context.selectionAt(static_cast<int>(text.find("aisons")));
    check("tap resolves the word", selection && selection->word == "maisons", selection ? selection->word : "(none)");
    check("tap resolves the sentence", selection && selection->sentence == "Les maisons étaient vieilles.", selection ? selection->sentence : "(none)");
    check("the sentence's place is known", selection && selection->sentenceStart == 0
          && Text::trim(text.substr(selection->sentenceStart, selection->sentenceEnd - selection->sentenceStart)) == selection->sentence);
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
    database.exec("DROP TABLE syncFiles");
    for (const char* column : {"groupID", "placeFraction", "placeSnippet", "placePending", "updatedAt", "remoteName"}) {
        database.exec(std::string("ALTER TABLE books DROP COLUMN ") + column);
    }
    database.exec("DROP TABLE bookGroups");
    database.setUserVersion(1);
    check("an older library migrates forward", Migrations::migrate(database) && database.userVersion() == 6
          && Statement(database, "SELECT lookupID FROM cardPractice").isValid()
          && Statement(database, "SELECT id FROM bookGroups").isValid()
          && Statement(database, "SELECT placeSnippet, updatedAt FROM books").isValid()
          && Statement(database, "SELECT word FROM lookupTombstones").isValid()
          && Statement(database, "SELECT remoteName FROM books").isValid()
          && Statement(database, "SELECT name FROM remoteBooks").isValid()
          && Statement(database, "SELECT folder, name, version, digest FROM syncFiles").isValid(), database.lastError());

    auto packs = env.packs.all();
    check("bundled pack is listed", packs.size() == 1 && packs[0].isBundled() && packs[0].isEnabled);
    env.packs.setEnabled(packs[0].id, false);
    check("packs can be disabled", env.packs.enabled().empty());
    env.packs.setEnabled(packs[0].id, true);
    {
        // Against a stand-in file: a remove that deleted the shipped file
        // would otherwise delete the iOS app's copy, and with it gone
        // checkBundledDictionary would skip, and the run would pass.
        const char* shipped = g_getenv("AIREADER_DATA_DIR");
        std::string data = shipped ? shipped : "";
        g_setenv("AIREADER_DATA_DIR", folder.c_str(), TRUE);
        Files::write(Paths::bundledDictionary(), "");
        env.packs.remove(packs[0]);
        check("the bundled pack can be removed, its file kept", env.packs.all().empty() && Files::exists(Paths::bundledDictionary()));
        if (shipped) g_setenv("AIREADER_DATA_DIR", data.c_str(), TRUE); else g_unsetenv("AIREADER_DATA_DIR");
    }
    env.packs.restoreBundled();
    env.packs.restoreBundled();
    packs = env.packs.all();
    check("restoring lists it once, as a new library has it", packs.size() == 1 && packs[0].isBundled() && packs[0].isEnabled
          && packs[0].name == "Bundled dictionary" && packs[0].languages() == "fr → ru");

    AiSettings ai{"mock://ai", "t1", "t2", "mock-medium"};
    env.settings.saveAi(ai);
    check("answer language defaults to Russian", ai.language == "Russian");
    ai.language = "English";
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

static void checkSyncParts() {
    // The web check expects the same names.
    check("a key's file is the same on every device",
          SyncParts::nameOf("le grand meaulnes|alain-fournier") == "f4.json" && SyncParts::nameOf("maisons\x01Les maisons.") == "95.json"
          && SyncParts::nameOf("été|") == "bb.json", SyncParts::nameOf("été|"));
    check("only the record files count", SyncParts::isPart("0a.json") && !SyncParts::isPart("0A.json")
          && !SyncParts::isPart(SyncParts::oldFile) && !SyncParts::isPart("._0a.json"));

    SyncDocument local;
    local.books = {bookRecord("a|", "2026-09-16T10:00:00Z"), bookRecord("b|", "2026-09-16T10:00:00Z")};
    local.lookups = {lookupRecord("un", "2026-09-16T10:00:00Z"), lookupRecord("deux", "2026-09-16T10:00:00Z")};
    auto parts = SyncParts::split(local);
    check("records are split by file and joined again", parts.size() > 1 && SyncParts::join(parts) == local.sorted());
    SyncDocument changed = local;
    changed.lookups[0].correct = 9;
    SyncDocument reordered = local;
    std::reverse(reordered.lookups.begin(), reordered.lookups.end());
    check("a fingerprint follows the records, not their order",
          SyncParts::digest(local).size() == 16 && SyncParts::digest(reordered) == SyncParts::digest(local)
          && SyncParts::digest(changed) != SyncParts::digest(local) && SyncParts::digest(SyncDocument()).empty());

    std::vector<SyncParts::File> known;
    std::vector<SyncParts::File> listed;
    for (auto file : SyncParts::fingerprints(local)) {
        file.version = "\"1\"";
        known.push_back(file);
        listed.push_back({file.name, file.version, ""});
    }
    known.push_back({SyncParts::oldFile, "\"o\"", ""});
    listed.push_back({"._0a.json", "\"x\"", ""});
    using Names = std::vector<std::string>;
    check("nothing is due when nothing changed", SyncParts::due(listed, known, local, {}).empty());
    std::string un = SyncParts::nameOf(local.lookups[0].key());
    check("a file whose records changed here is due", SyncParts::due(listed, known, changed, {}) == Names{un});
    auto moved = listed;
    moved[0].version = "\"2\"";
    check("a file changed on the server is due", SyncParts::due(moved, known, local, {}) == Names{moved[0].name});
    check("a file gone from the server is due",
          SyncParts::due(std::vector<SyncParts::File>(listed.begin() + 1, listed.end()), known, local, {}) == Names{listed[0].name});
    moved[0].version = "";
    check("a file the server gives no version is always due", SyncParts::due(moved, known, local, {}) == Names{moved[0].name});
    check("every file is due to a device that knows none", SyncParts::due(listed, {}, local, {}).size() == known.size() - 1);
    SyncDocument old;
    old.lookups = {lookupRecord("trois", "2026-09-16T09:00:00Z")};
    check("the old file's records make their files due",
          SyncParts::due(listed, known, local, old) == Names{SyncParts::nameOf(old.lookups[0].key())});

    SyncDocument theirs;
    theirs.lookups = {lookupRecord("un", "2026-09-16T09:00:00Z", "b|")};
    std::string fresh = SyncParts::nameOf(old.lookups[0].key());
    auto merged = SyncParts::merge({{un, theirs}, {fresh, {}}}, changed, old);
    auto inUn = [&](const SyncDocument& document) {
        for (const auto& record : document.lookups) if (record.word == "un") return record;
        return SyncDocument::LookupRecord();
    };
    check("a due file is merged with the records here that belong in it",
          merged.size() == 2 && merged[0].name == un && inUn(merged[0].records).correct == 9 && inUn(merged[0].records).book == "b|");
    check("and with the old file's", merged[1].name == fresh && merged[1].records.lookups.size() >= 1
          && SyncParts::split(merged[1].records).size() == 1);

    auto remembered = SyncParts::remember(known, {{un, "\"3\"", "d"}, {fresh, "", ""}, {"ff.json", "", "e"}});
    std::map<std::string, SyncParts::File> byName;
    for (const auto& file : remembered) byName[file.name] = file;
    check("remembering takes the settled files and keeps the rest",
          byName[un].version == "\"3\"" && byName.count(SyncParts::oldFile) && byName.count("ff.json") && !byName.count(fresh)
          && remembered.size() == known.size() + 1);
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
/// name one, in a folder of its own: a library an older version left in the
/// one file is moved over by the first device to sync, reaches a second
/// device, and from then on a change travels as the one file it falls in.
/// A device not yet updated still gets its changes through the old file.
static void checkSyncServer(const std::string& folder) {
    const char* base = g_getenv("AIREADER_SYNC_URL");
    if (!base) return;
    SyncSettings settings{std::string(base) + "/records-" + std::to_string(getpid()),
                          g_getenv("AIREADER_SYNC_USER") ? g_getenv("AIREADER_SYNC_USER") : "",
                          g_getenv("AIREADER_SYNC_PASSWORD") ? g_getenv("AIREADER_SYNC_PASSWORD") : ""};
    struct Pass {
        Sync::Fetched fetched;
        Sync::Round round;
        Sync::Sent sent;
    };
    // A round as the runner makes it, without the threads.
    auto sync = [&](Env& env) {
        Pass pass;
        try {
            pass.fetched = Sync::fetch(settings, Sync::gather(env, settings));
            pass.round = Sync::reconcile(env, pass.fetched);
            pass.sent = Sync::send(settings, pass.round);
            Sync::record(env, settings, pass.round, pass.sent);
        } catch (const std::exception& failure) {
            pass.sent.error = failure.what();
        }
        return pass;
    };
    auto word = [](Env& env, const std::string& text) {
        for (const auto& lookup : env.lookups.all()) if (lookup.word == text) return lookup;
        return Lookup();
    };

    SyncDocument old;
    old.books = {bookRecord("le grand meaulnes|alain-fournier", "2026-09-16T10:00:00Z", "Série", 2)};
    old.lookups = {lookupRecord("un", "2026-09-16T10:00:00Z"), lookupRecord("deux", "2026-09-16T10:00:00Z")};
    try {
        WebDav::upload(settings.oldFileUrl(), old.dump(), settings);
    } catch (const std::exception& failure) {
        check("sync server takes the old file", false, failure.what());
        return;
    }

    Database first(Files::join(folder, "records-a.sqlite3"));
    Migrations::migrate(first);
    Env a(first, Files::join(folder, "records-a.ini"));
    Book draft;
    draft.title = "Le Grand Meaulnes";
    draft.author = "Alain-Fournier";
    draft.path = Files::join(folder, "meaulnes.epub");
    long long id = a.library.add(draft);
    auto moved = sync(a);
    auto book = a.library.find(id);
    check("the first sync moves the old file over", moved.sent.error.empty() && moved.sent.files.size() == SyncParts::split(old).size()
          && a.lookups.all().size() == 2 && book && book->groupId && book->place && book->place->chapter == 2, moved.sent.error);
    auto quiet = sync(a);
    check("then a sync reads and sends nothing", quiet.sent.error.empty() && quiet.fetched.remote.empty() && quiet.sent.files.empty(),
          quiet.sent.error + " " + std::to_string(quiet.fetched.remote.size()) + " read");

    Database second(Files::join(folder, "records-b.sqlite3"));
    Migrations::migrate(second);
    Env b(second, Files::join(folder, "records-b.ini"));
    auto joined = sync(b);
    check("a second device receives the records and sends nothing", joined.sent.error.empty() && joined.sent.files.empty()
          && b.lookups.all().size() == 2, joined.sent.error);
    b.cards.record(word(b, "un").id, true);
    auto practised = sync(b);
    std::string un = SyncParts::nameOf(lookupRecord("un", "").key());
    check("a change there sends only its file", practised.sent.error.empty() && practised.fetched.remote.size() == 1
          && practised.sent.files.size() == 1 && practised.sent.files[0].name == un, practised.sent.error);
    auto received = sync(a);
    auto cards = a.cards.all();
    bool counted = false;
    for (const auto& card : cards) counted = counted || (card.front == "un" && card.correct == 1);
    check("and the first device reads only that file", received.sent.error.empty() && received.fetched.remote.size() == 1
          && received.sent.files.empty() && counted, received.sent.error);

    // An older version on a third device writes the old file again.
    old.lookups.push_back(lookupRecord("trois", "2026-09-17T10:00:00Z"));
    WebDav::upload(settings.oldFileUrl(), old.dump(), settings);
    auto passed = sync(a);
    check("a change to the old file still comes through", passed.sent.error.empty() && a.lookups.all().size() == 3
          && passed.sent.files.size() == 1 && passed.round.applied.lookups == 1, passed.sent.error);
    check("and is read once", sync(a).fetched.remote.empty());
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
    auto files = WebDav::entries(listing);
    check("listing gives files, decoded, without folders",
          files.size() == 1 && files[0].name == "Saint-Exupéry - Vol de nuit.epub", files.empty() ? "" : files[0].name);
    auto versioned = WebDav::entries(
        "<d:multistatus xmlns:d=\"DAV:\"><d:response><d:href>/dav/aireader-sync/0a.json</d:href><d:propstat><d:prop>"
        "<d:getetag>&quot;5f1c&quot;</d:getetag><d:resourcetype/></d:prop></d:propstat></d:response></d:multistatus>");
    check("listing gives each file's version", versioned.size() == 1 && versioned[0].name == "0a.json" && versioned[0].version == "\"5f1c\"",
          versioned.empty() ? "" : versioned[0].version);
    check("names escape for a url", WebDav::escape("Vol de nuit é.epub") == "Vol%20de%20nuit%20%C3%A9.epub");
}

/// Three devices and one server: the first sends its book; the second sees
/// it in the folder, fetches it on request and removes it again; the third,
/// which has the same book already, only takes note of the name. Then the
/// file is deleted from the folder. Needs AIREADER_SYNC_URL and a book on the
/// command line.
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
    auto record = [](Env& env, LibrarySync::Outcome outcome) {
        LibrarySync::record(env, outcome);
        return outcome;
    };
    auto round = [&](Env& env) { return record(env, LibrarySync::exchange(settings, env.library.all())); };

    auto first = device("first");
    Env a(*first, Paths::settings());
    Files::copy(epub, Files::join(Paths::books(), "livre.epub"));
    a.library.refresh(Paths::bookFolders());
    auto sent = round(a);
    std::string name = sent.named.empty() ? "" : sent.named[0].second;
    check("first device sends its book", sent.error.empty() && sent.sent == 1 && a.library.remoteNames().count(name), sent.error);
    auto quiet = round(a);
    check("first device then has nothing to do", quiet.error.empty() && quiet.sent == 0 && quiet.named.empty(), quiet.error);

    auto second = device("second");
    Env b(*second, Paths::settings());
    auto listed = round(b);
    check("second device lists it without fetching it", listed.error.empty() && listed.sent == 0 && b.library.all().empty()
          && b.library.remoteNames().count(name), listed.error);
    auto fetched = record(b, LibrarySync::fetch(settings, b.library.all(), name));
    auto shelf = b.library.all();
    check("second device fetches it on request", fetched.error.empty() && fetched.received.size() == 1
          && shelf.size() == 1 && shelf[0].remoteName == name && Files::exists(shelf[0].path), fetched.error);
    auto twice = LibrarySync::fetch(settings, b.library.all(), name);
    check("a book on the shelf is not fetched twice", !twice.error.empty() && twice.received.empty() && b.library.all().size() == 1);
    check("second device then has nothing to do", round(b).sent == 0);
    b.library.remove(shelf[0].id);
    Files::remove(shelf[0].path);
    check("a book removed here stays in the folder", round(b).error.empty() && b.library.remoteNames().count(name));

    auto third = device("third");
    Env c(*third, Paths::settings());
    Files::copy(epub, Files::join(Paths::books(), "same.epub"));
    c.library.refresh(Paths::bookFolders());
    auto matched = round(c);
    auto books = c.library.all();
    check("a book already here is recognised, not fetched or sent", matched.error.empty() && matched.received.empty()
          && matched.sent == 0 && books.size() == 1 && books[0].remoteName == name, matched.error);

    auto deleted = record(c, LibrarySync::remove(settings, name));
    check("deleting the file from the folder", deleted.error.empty() && c.library.remoteNames().empty(), deleted.error);
    auto after = round(a);
    check("a deleted file is not sent again", after.error.empty() && after.sent == 0 && a.library.remoteNames().empty(), after.error);
    auto missing = record(b, LibrarySync::fetch(settings, b.library.all(), name));
    check("fetching a deleted file says so", !missing.error.empty() && missing.gone.size() == 1 && b.library.all().empty(), missing.error);
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

static void checkBundledDictionary(const std::string& folder) {
    std::string path = Paths::bundledDictionary();
    if (!Files::exists(path)) {
        std::printf("skip  bundled dictionary (set AIREADER_DATA_DIR to the folder holding dictionary.sqlite3)\n");
        return;
    }
    {
        // The record goes by hand: remove() is checked against a stand-in
        // (checkDatabase), and this file is the iOS app's own copy.
        Files::ensureDirectory(folder);
        Database database(Files::join(folder, "library.sqlite3"));
        Migrations::migrate(database);
        DictionaryPacks packs(database);
        database.exec("DELETE FROM dictionaryPacks");
        packs.restoreBundled();
        DictionaryLookup again = DictionaryDatabase::shared().lookup("maisons", packs.enabled());
        check("a restored bundled pack is searched again", !again.articles.empty() && again.articles[0].lemma == "maison", again.summary());
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

    ToolRunner::Tools tools{{corpus, upTo, std::nullopt}, {}, {}};
    std::vector<ChatMessage> xray = XRayPrompt::messages("Nobody", corpus->search("Nobody", 5, upTo), true, "Russian");
    ChatMessage answer = ToolRunner::converse(mock, xray, Json(std::vector<Json>{SearchTool::tool()}), false, tools);
    check("tool runner answers a search call", xray.size() == 4 && xray[3].role == "tool" && Text::contains(*xray[3].content, "No passage")
          && Text::contains(answer.content.value_or(""), "не встречается"), answer.content.value_or(""));
    std::vector<ChatMessage> found = XRayPrompt::messages("Meaulnes", corpus->search("Meaulnes", 5, upTo), true, "Russian");
    ChatMessage known = ToolRunner::converse(mock, found, Json(std::vector<Json>{SearchTool::tool()}), false, tools);
    check("x-ray answers from the passages read so far", Text::contains(known.content.value_or(""), "1 отрывках"), known.content.value_or(""));
}

/// Makes an EPUB of a PDF, FB2, text or HTML file given on the command line,
/// shows what came of it, and hands back the EPUB for the checks an EPUB gets.
static std::string checkConvertedFile(const std::string& path, const std::string& folder) {
    Files::ensureDirectory(folder);
    std::string error;
    std::string epub = Files::extension(path) == "pdf" ? PdfImporter::convert(path, folder, &error)
                                                       : BookImporter::convert(path, folder, &error);
    check("the file becomes an epub", !epub.empty(), error.empty() ? epub : error);
    if (epub.empty()) return path;
    auto document = EpubLoader::load(epub, &error, false);
    if (!document) return epub;
    for (size_t i = 0; i < document->chapters.size() && i < 3; ++i) {
        std::string text = document->chapters[i].text;
        std::printf("      chapter %zu: %s\n", i + 1, text.substr(0, 600).c_str());
    }
    return epub;
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
        // A word of the book's own, so a book in any language is searched:
        // the first in its first chapter that has one, since a cover's has
        // only a picture.
        std::string word;
        for (size_t i = 0; word.empty() && i < document->chapters.size(); ++i) {
            const std::string& chapter = document->chapters[i].text;
            const char* p = chapter.c_str();
            while (*p && !g_unichar_isalpha(g_utf8_get_char(p))) p = g_utf8_next_char(p);
            const char* start = p;
            while (*p && g_unichar_isalpha(g_utf8_get_char(p))) p = g_utf8_next_char(p);
            word.assign(start, p);
        }
        BookCorpus corpus({book});
        auto hits = corpus.search(word, 5);
        check("corpus searches the epub from disk", !word.empty() && !hits.empty() && corpus.errors().empty(),
              "“" + word + "”: " + (hits.empty() ? "" : hits[0].excerpt));
    }
}

/// The web check's glossary cases, with the same expectations but one: Pango
/// breaks a word at an apostrophe, where the other apps' segmenters do not,
/// so here a tap on "l'homme" finds "l" or "homme", and the glossary has those.
static void checkGlossary() {
    std::vector<std::string> forms;
    for (const auto& word : BookWords::collect({"L’homme dit : c'est-à-dire 42 fois, l'homme !"}, "fr")) forms.push_back(word.form);
    check("glossary forms follow the word boundaries", Text::join(forms, " ") == "l homme dit c est à dire fois", Text::join(forms, " "));
    BookWord chat = BookWords::collect({"Le chat un.\nLe chat deux.", "Le chat trois.\nLe chat quatre."}, "fr")[1];
    check("a form keeps its first three places", Text::join(chat.examples, " | ") == "Le chat un. | Le chat deux. | Le chat trois.",
          Text::join(chat.examples, " | "));
    BookWord around = BookWords::collect({repeated("mot ", 10) + "chat" + repeated(" mot", 10) + "."}, "fr")[1];
    check("an example is the words around it", around.examples.size() == 1
          && around.examples[0] == "…" + repeated("mot ", 8) + "chat" + repeated(" mot", 8) + "…", around.examples.empty() ? "" : around.examples[0]);

    std::vector<BookWord> words = {{"maisons", "maisons", {}}, {"paris", "Paris", {"à Paris en hiver"}}, {"est", "est", {}}};
    check("glossary question", GlossaryPrompt::question({words[1]}) == "1. Paris\n   — à Paris en hiver");
    check("glossary prompt names the language", Text::contains(GlossaryPrompt::system("Russian"), "на языке «Russian»:\n{\"words\": [{\"n\": "));
    std::string answer = "```json\n{\"words\": [\n"
        "{\"n\": 1, \"lemma\": \"maison\", \"form_note\": \"мн. ч.\", \"meaning\": \"дома\"},\n"
        "{\"n\": \"2\", \"lemma\": \"Paris\", \"form_note\": \"\", \"meaning\": \"Париж,\\t\\\"столица\\\"\"},\n"
        "{\"n\": 9, \"lemma\": \"x\", \"meaning\": \"вне списка\"},\n"
        "{\"n\": 3, \"lemma\": \"être\", \"meaning\": \"\"}\n]}\n```";
    auto definitions = GlossaryPrompt::definitions(answer, words);
    check("glossary answer becomes definitions", definitions && definitions->size() == 2 && definitions->at("maisons") == "maison (мн. ч.): дома"
          && definitions->at("paris") == "Париж, 'столица'", definitions && definitions->count("paris") ? definitions->at("paris") : "");
    check("an unreadable glossary answer is no answer", !GlossaryPrompt::definitions("Sorry, I cannot help.", words));
    ChatMessage mocked = MockAI::reply({ChatMessage::system(GlossaryPrompt::system("Russian")), ChatMessage::user(GlossaryPrompt::question(words))});
    auto defined = GlossaryPrompt::definitions(mocked.content.value_or(""), words);
    check("mock defines every word of a batch", defined && defined->size() == 3 && defined->at("paris") == "«Paris» в книге (макет)",
          mocked.content.value_or(""));
}

/// The word list a run appends to, and the dictionary made of it.
static void checkGlossaryList(const std::string& folder) {
    g_setenv("AIREADER_HOME", folder.c_str(), TRUE);
    Paths::prepare();
    Database database(Files::join(folder, "library.sqlite3"));
    Migrations::migrate(database);
    DictionaryPacks packs(database);
    Book book;
    book.title = "Le Petit Livre";
    AiSettings settings;
    settings.endpoint = "mock://ai";
    settings.model = "mock-medium";

    std::vector<BookWord> words = BookWords::collect({"Le chat arrive.\nLa maison attend."}, "fr");
    Glossary::append(book, settings, Glossary::define(settings, {words.begin(), words.begin() + 3}));
    Glossary::install(packs, book);
    check("a glossary keeps what a run defined", Glossary::definedForms(book) == std::set<std::string>{"le", "chat", "arrive"});
    Glossary::append(book, settings, Glossary::define(settings, {words.begin() + 3, words.end()}));
    Glossary::install(packs, book);
    int named = 0;
    std::vector<DictionaryPack> glossary;
    for (const auto& pack : packs.all()) {
        if (pack.name == "Le Petit Livre glossary") {
            ++named;
            glossary.push_back(pack);
        }
    }
    DictionaryLookup found = DictionaryDatabase::shared().lookup("Attend", glossary);
    check("installing again replaces the dictionary, read afresh", named == 1 && found.articles.size() == 1
          && found.articles[0].senses[0] == "«attend» в книге (макет)", found.summary());
}

int main(int argc, char** argv) {
    g_thread_init(nullptr);
    ChatApi::initialize();
    checkJson();
    checkEpubPackage();
    checkEpubNavigation();
    checkHtmlText();
    checkCharset();
    checkScanner();
    checkTextBook();
    checkHtmlBook();
    checkFb2();
    checkLanguage();
    checkDictionaryLookup();
    checkMock();
    checkGlossary();
    checkQuirks();
    checkChatPrompt();
    checkWebSearch();
    checkBookSearch();
    checkContextTool();
    checkWordContext();
    checkCards();
    checkPaginator();

    char pattern[] = "/tmp/aireader-check-XXXXXX";
    const char* folder = mkdtemp(pattern);
    std::string scratch = folder ? folder : "/tmp";
    checkReadingPlace();
    checkRemoteBooks();
    checkSyncDocument();
    checkSyncParts();
    checkDatabase(scratch);
    checkSyncStore(scratch);
    checkSyncServer(scratch);
    checkWordLists(Files::join(scratch, "home"));
    checkConverters(Files::join(scratch, "home"));
    checkAddFile(Files::join(scratch, "home"));
    checkGlossaryList(Files::join(scratch, "glossary"));
    checkPdf(Files::join(scratch, "pdf-sample"));
    checkBookImporter(Files::join(scratch, "import"));
    checkBundledDictionary(Files::join(scratch, "bundled"));
    // A PDF, FB2, text or HTML file is checked as the EPUB the library makes of it.
    std::string book = argc > 1 ? argv[1] : "";
    if (Files::extension(book) == "pdf" || BookImporter::reads(book)) book = checkConvertedFile(book, Files::join(scratch, "converted"));
    if (argc > 1) checkLibraryServer(Files::join(scratch, "library"), book);
    if (argc > 1) checkEpub(book);
    if (argc > 2) renderPage(book, argv[2], argc > 3 ? std::atoi(argv[3]) : 0, argc > 4 ? std::atoi(argv[4]) : 0);

    std::printf("%s\n", failures ? "FAILED" : "all checks passed");
    return failures ? 1 : 0;
}
