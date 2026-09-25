#pragma once

#include <string>
#include <vector>

/// Where the app keeps its things. On the Kindle everything lives under
/// `/mnt/us/aireader`, which is the USB-visible partition, so books and
/// dictionaries can be copied in from a computer.
namespace Paths {

/// `$AIREADER_HOME`, or `~/.aireader` when unset. `launch.sh` sets it on the Kindle.
std::string home();
std::string books();
std::string dictionaries();
std::string database();
std::string settings();
/// The cards written for Anki, beside the books so a computer can take it.
std::string ankiCards();
/// Folders scanned for `.epub` files.
std::vector<std::string> bookFolders();
/// Where the file picker starts: the USB-visible partition on the Kindle,
/// the home folder on a desktop.
std::string browseRoot();
/// The dictionary that ships with the app, beside the executable or in
/// `$AIREADER_DATA_DIR`.
std::string bundledDictionary();
bool onKindle();
/// Creates the folders above.
void prepare();

}  // namespace Paths
