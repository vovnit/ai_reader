cmake_minimum_required(VERSION 3.21)
project(aireader-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NATIVE REQUIRED IMPORTED_TARGET
    glib-2.0 gthread-2.0 pangocairo gdk-pixbuf-2.0 sqlite3 zlib libcurl)

# Everything below the views is the Kindle app's: its support, domain,
# services and feature state, plus what ../Core shares with iOS. Globbed, so
# a file added there is picked up here too. Only the GTK views, the e-ink
# panel and the on-screen keyboard stay behind.
set(KINDLE ${CMAKE_CURRENT_SOURCE_DIR}/../AIReaderKindle/src)
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../Core/Sources/AIReaderCore)
file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS
    ${CORE}/*.cpp
    ${KINDLE}/Support/*.cpp
    ${KINDLE}/Domain/*.cpp
    ${KINDLE}/Services/*.cpp
    ${KINDLE}/Features/*Feature.cpp)
list(FILTER SHARED_SOURCES EXCLUDE REGEX "Services/EinkDisplay\\.cpp$")
list(APPEND SHARED_SOURCES ${KINDLE}/Features/Common/SyncRunner.cpp)

set(DATA_DIR ${CMAKE_INSTALL_PREFIX}/share/aireader)

add_library(aireader-core STATIC ${SHARED_SOURCES})
target_include_directories(aireader-core PUBLIC ${CORE} ${KINDLE})
target_compile_definitions(aireader-core PUBLIC
    AIREADER_DATA_DIR="${DATA_DIR}"
    AIREADER_VERSION="${PROJECT_VERSION}")
target_link_libraries(aireader-core PUBLIC PkgConfig::NATIVE)
# The Kindle's GLib is 2.29, so the shared code uses its thread calls, which
# a desktop GLib marks deprecated.
target_compile_options(aireader-core PUBLIC -Wno-deprecated-declarations)

file(GLOB_RECURSE VIEW_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)
add_executable(aireader-qt ${VIEW_SOURCES})
target_include_directories(aireader-qt PRIVATE src)
# GLib's headers use `signals` as a name; Qt's macro of that name would break them.
target_compile_definitions(aireader-qt PRIVATE QT_NO_KEYWORDS)
target_link_libraries(aireader-qt PRIVATE aireader-core Qt6::Widgets Qt6::Test)

install(TARGETS aireader-qt RUNTIME DESTINATION bin)
install(FILES ../AIReader/AIReader/Resources/dictionary.sqlite3 DESTINATION share/aireader)
install(FILES packaging/aireader.desktop DESTINATION share/applications)
install(FILES packaging/aireader.svg DESTINATION share/icons/hicolor/scalable/apps)
