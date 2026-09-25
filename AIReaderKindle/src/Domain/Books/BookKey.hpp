#pragma once

#include <string>

/// The name a book goes by across devices, since neither the row id nor the
/// file is the same on two of them: its title and author, normalized.
namespace BookKey {

std::string make(const std::string& title, const std::string& author);

}  // namespace BookKey
