#pragma once

#include "../Domain/AI/WordExplanation.hpp"
#include "ToolRunner.hpp"

#include <string>

/// Explains a word by handing the dictionary's answer to the model and letting
/// it ask for more entries — or search the book — until it can commit to a
/// meaning. Runs on a worker thread; throws `ChatApi::Error`.
namespace WordExplainer {

WordExplanation explain(
    const std::string& word,
    const std::string& sentence,
    const AiSettings& settings,
    const ToolRunner::Tools& tools);

}  // namespace WordExplainer
