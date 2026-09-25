project('aireader', 'cpp',
  version: '0.1.0',
  meson_version: '>=1.0',
  default_options: ['cpp_std=c++17', 'warning_level=2', 'buildtype=release'])

glib_dep = dependency('glib-2.0')
pango_dep = dependency('pangocairo')
pixbuf_dep = dependency('gdk-pixbuf-2.0')
sqlite_dep = dependency('sqlite3')
zlib_dep = dependency('zlib')
curl_dep = dependency('libcurl')
gtk_dep = dependency('gtk+-2.0')
threads_dep = dependency('threads')
# GLib 2.29 keeps its thread support in a separate library that the Kindle
# firmware ships without a .pc file.
cpp = meson.get_compiler('cpp')
gthread_dep = dependency('gthread-2.0', required: false)
if not gthread_dep.found()
  gthread_dep = cpp.find_library('gthread-2.0', required: false)
endif

datadir = get_option('prefix') / get_option('datadir') / 'aireader'
# GTK+ 2's own headers trip GLib's deprecation warnings; nothing to fix here.
add_project_arguments(
  '-Wno-deprecated-declarations',
  '-DAIREADER_DATA_DIR="' + datadir + '"',
  '-DAIREADER_APP_ID="' + get_option('kindle_app_id') + '"',
  '-DAIREADER_VERSION="' + meson.project_version() + '"',
  language: 'cpp')

# Koxtoolchain ships a GCC newer than the Kindle supports, which is fine so
# long as the C++ runtime is linked statically.
link_args = []
if meson.is_cross_build()
  link_args += ['-static-libstdc++', '-static-libgcc']
endif

# Shared with the iOS app, which builds it as the Swift package in ../Core.
# Its files are included by their path from there: "Support/Json.hpp".
shared = files(
  '../Core/Sources/AIReaderCore/Domain/Sync/SyncDocument.cpp',
  '../Core/Sources/AIReaderCore/Support/Json.cpp',
  '../Core/Sources/AIReaderCore/Support/JsonParser.cpp',
)
shared_inc = include_directories('../Core/Sources/AIReaderCore')

support = files(
  'src/Support/Async.hpp',
  'src/Support/Files.cpp',
  'src/Support/Inflate.cpp',
  'src/Support/Text.cpp',
  'src/Support/TextFile.cpp',
  'src/Support/XmlScanner.cpp',
  'src/Support/ZipArchive.cpp',
)

domain = files(
  'src/Domain/AI/ChatMessage.cpp',
  'src/Domain/AI/ChatPrompt.cpp',
  'src/Domain/AI/DictionaryTool.cpp',
  'src/Domain/AI/ExplanationPrompt.cpp',
  'src/Domain/AI/MockAI.cpp',
  'src/Domain/AI/RequestQuirks.cpp',
  'src/Domain/AI/SearchTool.cpp',
  'src/Domain/AI/ToolSchema.cpp',
  'src/Domain/AI/WebSearchTool.cpp',
  'src/Domain/AI/WordExplanation.cpp',
  'src/Domain/AI/XRayPrompt.cpp',
  'src/Domain/Books/BookKey.cpp',
  'src/Domain/Books/EpubNavigation.cpp',
  'src/Domain/Books/EpubPackage.cpp',
  'src/Domain/Books/HtmlText.cpp',
  'src/Domain/Books/LanguageDetector.cpp',
  'src/Domain/Books/ReadingPlace.cpp',
  'src/Domain/Books/RemoteBookName.cpp',
  'src/Domain/Cards/AnkiExport.cpp',
  'src/Domain/Cards/Card.cpp',
  'src/Domain/Cards/MatchRound.cpp',
  'src/Domain/Dictionary/DictionaryLookup.cpp',
  'src/Domain/Dictionary/WordNormalizer.cpp',
  'src/Domain/Formats/DSLDictionaryReader.cpp',
  'src/Domain/Formats/DelimitedDictionaryReader.cpp',
  'src/Domain/Formats/DictionaryConverter.cpp',
  'src/Domain/Formats/DictionaryFormat.cpp',
  'src/Domain/Formats/StarDictReader.cpp',
  'src/Domain/Formats/XDXFDictionaryReader.cpp',
  'src/Domain/Reading/Illustrations.cpp',
  'src/Domain/Reading/Paginator.cpp',
  'src/Domain/Reading/WordContext.cpp',
  'src/Domain/Search/BookSearch.cpp',
)

services = files(
  'src/Services/BookCorpus.cpp',
  'src/Services/CardStore.cpp',
  'src/Services/ChatApi.cpp',
  'src/Services/Database.cpp',
  'src/Services/DictionaryDatabase.cpp',
  'src/Services/DictionaryPackWriter.cpp',
  'src/Services/DictionaryPacks.cpp',
  'src/Services/EinkDisplay.cpp',
  'src/Services/EpubLoader.cpp',
  'src/Services/GroupStore.cpp',
  'src/Services/Http.cpp',
  'src/Services/LibraryStore.cpp',
  'src/Services/LibrarySync.cpp',
  'src/Services/LookupCache.cpp',
  'src/Services/Migrations.cpp',
  'src/Services/Paths.cpp',
  'src/Services/Settings.cpp',
  'src/Services/Sync.cpp',
  'src/Services/SyncStore.cpp',
  'src/Services/ToolRunner.cpp',
  'src/Services/WebDav.cpp',
  'src/Services/WebSearch.cpp',
  'src/Services/WordExplainer.cpp',
)

features = files(
  'src/Features/Chat/ChatFeature.cpp',
  'src/Features/Chat/ChatView.cpp',
  'src/Features/Common/Dialogs.cpp',
  'src/Features/Common/Keyboard.cpp',
  'src/Features/Common/Navigator.cpp',
  'src/Features/Common/SyncRunner.cpp',
  'src/Features/Common/Theme.cpp',
  'src/Features/Common/Widgets.cpp',
  'src/Features/Contents/ContentsView.cpp',
  'src/Features/Dictionaries/DictionariesFeature.cpp',
  'src/Features/Dictionaries/DictionariesView.cpp',
  'src/Features/FilePicker/FilePickerFeature.cpp',
  'src/Features/FilePicker/FilePickerView.cpp',
  'src/Features/Library/GroupView.cpp',
  'src/Features/Library/LibraryFeature.cpp',
  'src/Features/Library/LibraryView.cpp',
  'src/Features/Lookup/EntryView.cpp',
  'src/Features/Lookup/LookupFeature.cpp',
  'src/Features/Lookup/LookupView.cpp',
  'src/Features/Match/MatchFeature.cpp',
  'src/Features/Match/MatchView.cpp',
  'src/Features/Menu/DisplayFeature.cpp',
  'src/Features/Menu/DisplayView.cpp',
  'src/Features/Menu/MenuView.cpp',
  'src/Features/Reader/ReaderFeature.cpp',
  'src/Features/Reader/ReaderView.cpp',
  'src/Features/Search/HitView.cpp',
  'src/Features/Search/SearchFeature.cpp',
  'src/Features/Search/SearchView.cpp',
  'src/Features/Settings/SettingsFeature.cpp',
  'src/Features/Settings/SettingsView.cpp',
  'src/Features/Words/WordsFeature.cpp',
  'src/Features/Words/WordsView.cpp',
  'src/Features/XRay/XRayFeature.cpp',
  'src/Features/XRay/XRayView.cpp',
)

app = files(
  'src/App/SmokeScript.cpp',
  'src/App/main.cpp',
)

core_deps = [glib_dep, gthread_dep, threads_dep, pango_dep, pixbuf_dep, sqlite_dep, zlib_dep, curl_dep]

# Everything below the views: portable, and what the check tool exercises.
core = static_library('aireader-core', shared + support + domain + services,
  include_directories: shared_inc,
  dependencies: core_deps)

executable('aireader', features + app,
  link_with: core,
  include_directories: shared_inc,
  dependencies: core_deps + [gtk_dep],
  link_args: link_args,
  install: true)

# Pure logic, run from the command line: prompt building, mock replies,
# dictionary formatting, parameter negotiation, EPUB reading.
executable('aireader-check', 'tools/check.cpp',
  link_with: core,
  include_directories: shared_inc,
  dependencies: core_deps,
  link_args: link_args)
