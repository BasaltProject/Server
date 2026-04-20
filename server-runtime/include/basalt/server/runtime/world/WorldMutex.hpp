#pragma once

#include <mutex>
#include <shared_mutex>

namespace basalt::server::runtime::world {

using WorldMutex = std::shared_mutex;

using WorldReadLock = std::shared_lock<WorldMutex>;

using WorldWriteLock = std::lock_guard<WorldMutex>;

} // namespace basalt::server::runtime::world
