#pragma once

#include <mutex>
#include <shared_mutex>

namespace basalt::server::runtime::world {

/// The one lock a world has, shared by everything inside it. One rather than one per collaborator:
/// splitting granularity changes race outcomes between the tick thread and the save and eviction
/// threads, which is a behaviour change even where parity is not involved.
///
/// Shared rather than plain because block reads take it: the frozen Kotlin read the chunk map off an
/// immutable snapshot the collector kept alive, and C++ has nowhere to put that snapshot.
using WorldMutex = std::shared_mutex;

/// What a reader takes: any number at once, and none while a writer holds it.
using WorldReadLock = std::shared_lock<WorldMutex>;

/// What a writer takes.
using WorldWriteLock = std::lock_guard<WorldMutex>;

} // namespace basalt::server::runtime::world
