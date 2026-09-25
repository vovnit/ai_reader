#pragma once

#include <string>
#include <vector>

/// The file name a book is given in the shared `Books` folder: author and
/// title, so it reads well in any file manager, with nothing a Kindle's FAT
/// partition would refuse. The same rule as the iOS app's
/// `Domain/Books/RemoteBookName.swift`.
namespace RemoteBookName {

extern const char* const folder;

/// A name none of `taken` has, compared without case, since some servers
/// ignore it.
std::string make(const std::string& title, const std::string& author, const std::vector<std::string>& taken);
bool isBook(const std::string& name);

}  // namespace RemoteBookName
