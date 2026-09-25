#pragma once

#include "Support/Json.hpp"

#include <string>

/// The JSON shape of a function tool with one required string argument, as
/// the chat completions API expects it.
namespace ToolSchema {

Json function(
    const std::string& name,
    const std::string& description,
    const std::string& argument,
    const std::string& argumentDescription);

/// The string argument out of a call's arguments, or `fallback` when the
/// model sent something else.
std::string argument(const std::string& arguments, const std::string& name, const std::string& fallback);

}  // namespace ToolSchema
