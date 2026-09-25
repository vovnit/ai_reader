#include "ToolSchema.hpp"

namespace ToolSchema {

Json function(
    const std::string& name,
    const std::string& description,
    const std::string& argument,
    const std::string& argumentDescription)
{
    Json value = Json::object();
    value.set("type", "string");
    value.set("description", argumentDescription);

    Json properties = Json::object();
    properties.set(argument, value);

    Json parameters = Json::object();
    parameters.set("type", "object");
    parameters.set("properties", properties);
    parameters.set("required", Json(std::vector<Json>{Json(argument)}));

    Json function = Json::object();
    function.set("name", name);
    function.set("description", description);
    function.set("parameters", parameters);

    Json tool = Json::object();
    tool.set("type", "function");
    tool.set("function", function);
    return tool;
}

std::string argument(const std::string& arguments, const std::string& name, const std::string& fallback) {
    auto json = Json::parse(arguments);
    if (json && json->at(name).isString()) return json->at(name).string();
    return fallback;
}

}  // namespace ToolSchema
