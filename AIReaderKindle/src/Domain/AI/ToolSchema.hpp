#pragma once

#include "Support/Json.hpp"

#include <string>
#include <vector>

/// The JSON shape of a function tool with one required string argument, as
/// the chat completions API expects it.
namespace ToolSchema {

/// `choices`, when given, are the only values the argument may take.
Json function(
    const std::string& name,
    const std::string& description,
    const std::string& argument,
    const std::string& argumentDescription,
    const std::vector<std::string>& choices = {});

/// The string argument out of a call's arguments, or `fallback` when the
/// model sent something else.
std::string argument(const std::string& arguments, const std::string& name, const std::string& fallback);

}  // namespace ToolSchema
