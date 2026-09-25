#pragma once

#include <optional>
#include <string>

/// DEFLATE decompression on top of zlib. ZIP entries store raw DEFLATE
/// streams; the dictionary packs store zlib streams, which are the same
/// payload wrapped in a header and a checksum.
namespace Inflate {

std::optional<std::string> raw(const std::string& data);
std::optional<std::string> zlib(const std::string& data);
/// Inflates a gzip stream by stepping over its header. StarDict's `.dict.dz`
/// and Lingvo's `.dsl.dz` are both plain gzip.
std::optional<std::string> gzip(const std::string& data);
bool isGzip(const std::string& data);

}  // namespace Inflate
