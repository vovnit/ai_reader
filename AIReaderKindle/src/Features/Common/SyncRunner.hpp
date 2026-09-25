#pragma once

#include "../../Services/Env.hpp"

#include <functional>
#include <memory>
#include <string>

/// One sync, run the way a screen needs it: the network on a worker thread,
/// the database on the main loop, the outcome delivered as a line of text.
/// Nothing happens when no server has been set up.
namespace SyncRunner {

/// `done` gets what the sync did, or why it could not, and whether it
/// failed. Dropped if `alive` has expired by then.
void run(Env& env, std::shared_ptr<bool> alive, std::function<void(const std::string& message, bool failed)> done);

}  // namespace SyncRunner
