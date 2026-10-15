#include <basalt/server/world/terrain/ProtoChunks.hpp>
#include <basalt/server/world/terrain/ProtoRegion.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <tuple>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

[[nodiscard]] std::int32_t xOf(const std::int64_t key) { return static_cast<std::int32_t>(key); }

[[nodiscard]] std::int32_t zOf(const std::int64_t key) { return static_cast<std::int32_t>(key >> 32); }

[[nodiscard]] std::int32_t distance(const std::int64_t first, const std::int64_t second) {
    return std::max(std::abs(xOf(first) - xOf(second)), std::abs(zOf(first) - zOf(second)));
}

void lower(std::unordered_map<std::int64_t, std::int32_t>& wants, const std::int64_t key, const std::int32_t priority) {
    const auto [at, inserted] = wants.try_emplace(key, priority);
    if (!inserted && priority < at->second) {
        at->second = priority;
    }
}

constexpr std::chrono::milliseconds kWaitPoll{1};

} // namespace

struct ProtoChunks::Entry {
    std::unique_ptr<realTerrainGenerator::GeneratedTerrain> terrain;
    ChunkStage stage = ChunkStage::Terrain;
    bool touched = false;
    std::uint64_t used = 0;
};

struct ProtoChunks::Request {
    std::int32_t priority = 0;
    bool resolved = false;
    bool waited = false;
};

struct ProtoChunks::Task {
    enum class Kind : std::uint8_t { Resolve, Terrain, Decorate, Finish };
    Kind kind = Kind::Terrain;
    std::int64_t key = 0;
    std::array<Chunk*, 9> region{};
    realTerrainGenerator::GeneratedTerrain* decorating = nullptr;
    std::optional<Stored> stored;
    std::unique_ptr<realTerrainGenerator::GeneratedTerrain> made;
    std::optional<Chunk> finishing;
};

std::int64_t ProtoChunks::key(const std::int32_t chunkX, const std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

ProtoChunks::ProtoChunks(Hooks hooks, const std::size_t capacity) : hooks_(std::move(hooks)), capacity_(capacity) {}

ProtoChunks::~ProtoChunks() = default;

void ProtoChunks::demand(const std::int32_t chunkX, const std::int32_t chunkZ, const std::int32_t priority) {
    const std::lock_guard lock{mutex_};
    const std::int64_t at = key(chunkX, chunkZ);
    const auto [request, inserted] = requests_.try_emplace(at, Request{.priority = priority});
    if (!inserted && priority >= request->second.priority) {
        return;
    }
    request->second.priority = priority;
    wantsStale_ = true;
}

std::size_t ProtoChunks::retainWhere(const std::function<bool(std::int32_t, std::int32_t)>& keep) {
    const std::lock_guard lock{mutex_};
    const std::size_t before = requests_.size();
    std::erase_if(requests_, [&](const auto& entry) {
        return !entry.second.waited && !busy_.contains(entry.first) && !keep(xOf(entry.first), zOf(entry.first));
    });
    if (requests_.size() != before) {
        wantsStale_ = true;
    }
    return before - requests_.size();
}

void ProtoChunks::clearDemands() {
    const std::lock_guard lock{mutex_};
    std::erase_if(requests_, [this](const auto& entry) { return !busy_.contains(entry.first); });
    wantsStale_ = true;
}

std::unordered_set<std::int64_t> ProtoChunks::pendingKeys() const {
    const std::lock_guard lock{mutex_};
    std::unordered_set<std::int64_t> keys;
    for (const auto& [at, request] : requests_) {
        keys.insert(at);
    }
    for (const Finished& done : finished_) {
        keys.insert(done.key);
    }
    return keys;
}

std::size_t ProtoChunks::pendingCount() const {
    const std::lock_guard lock{mutex_};
    return requests_.size() + finished_.size();
}

std::size_t ProtoChunks::protoCount() const {
    const std::lock_guard lock{mutex_};
    return entries_.size();
}

bool ProtoChunks::busy() const {
    const std::lock_guard lock{mutex_};
    return !busy_.empty();
}

void ProtoChunks::refreshWants() {
    if (!wantsStale_) {
        return;
    }
    wantsStale_ = false;
    decorationWants_.clear();
    terrainWants_.clear();
    unresolved_.clear();
    std::vector<std::pair<std::int32_t, std::int64_t>> resolved;
    resolved.reserve(requests_.size());
    for (const auto& [at, request] : requests_) {
        if (!request.resolved) {
            unresolved_.push_back(at);
            continue;
        }
        resolved.emplace_back(request.priority, at);
    }
    std::ranges::sort(resolved, [](const auto& left, const auto& right) {
        return std::tuple{left.first, xOf(left.second), zOf(left.second)} <
               std::tuple{right.first, xOf(right.second), zOf(right.second)};
    });
    // parity: `ChunkGenerationTask` order is executor-dependent in vanilla.
    decorationOrder_.clear();
    for (const auto& [priority, at] : resolved) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                const std::int64_t around = key(xOf(at) + dx, zOf(at) + dz);
                if (full_.contains(around) || decorationWants_.contains(around)) {
                    continue;
                }
                const auto entry = entries_.find(around);
                if (entry != entries_.end() && entry->second->stage == ChunkStage::Decorated) {
                    continue;
                }
                decorationWants_.emplace(around, priority);
                decorationOrder_.push_back(around);
            }
        }
    }
    for (const std::int64_t at : decorationOrder_) {
        const std::int32_t priority = decorationWants_.at(at);
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                const std::int64_t around = key(xOf(at) + dx, zOf(at) + dz);
                if (!entries_.contains(around) && !full_.contains(around)) {
                    lower(terrainWants_, around, priority);
                }
            }
        }
    }
}

bool ProtoChunks::decorationMayRun(const std::int64_t chunk, const std::unordered_set<std::int64_t>& earlier) const {
    const auto entry = entries_.find(chunk);
    if (entry == entries_.end() || entry->second->stage != ChunkStage::Terrain || busy_.contains(chunk)) {
        return false;
    }
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            const std::int64_t around = key(xOf(chunk) + dx, zOf(chunk) + dz);
            const bool adjacent = std::abs(dx) <= 1 && std::abs(dz) <= 1;
            if (adjacent && !entries_.contains(around) && !full_.contains(around)) {
                return false;
            }
            if ((dx != 0 || dz != 0) && (decorating_.contains(around) || earlier.contains(around))) {
                return false;
            }
        }
    }
    return std::ranges::none_of(unresolved_, [chunk](const std::int64_t at) { return distance(at, chunk) <= 3; });
}

std::optional<ProtoChunks::Task> ProtoChunks::pick() {
    for (auto& [at, request] : requests_) {
        if (!request.resolved || busy_.contains(at)) {
            continue;
        }
        const auto entry = entries_.find(at);
        if (entry == entries_.end() || entry->second->stage != ChunkStage::Decorated) {
            continue;
        }
        bool ready = true;
        for (std::int32_t dx = -1; dx <= 1 && ready; ++dx) {
            for (std::int32_t dz = -1; dz <= 1 && ready; ++dz) {
                const std::int64_t around = key(xOf(at) + dx, zOf(at) + dz);
                const auto neighbour = entries_.find(around);
                ready = full_.contains(around) ||
                        (neighbour != entries_.end() && neighbour->second->stage == ChunkStage::Decorated);
            }
        }
        if (!ready) {
            continue;
        }
        Task task{.kind = Task::Kind::Finish, .key = at};
        task.finishing.emplace(std::move(entry->second->terrain->chunk()));
        entries_.erase(entry);
        full_.insert(at);
        busy_.insert(at);
        wantsStale_ = true;
        return task;
    }

    refreshWants();
    std::unordered_set<std::int64_t> earlier;
    for (const std::int64_t at : decorationOrder_) {
        if (!decorationMayRun(at, earlier)) {
            earlier.insert(at);
            continue;
        }
        Task task{.kind = Task::Kind::Decorate, .key = at};
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                const auto neighbour = entries_.find(key(xOf(at) + dx, zOf(at) + dz));
                Chunk* chunk = nullptr;
                if (neighbour != entries_.end()) {
                    chunk = &neighbour->second->terrain->chunk();
                    neighbour->second->used = ++clock_;
                }
                task.region[static_cast<std::size_t>(((dx + 1) * 3) + dz + 1)] = chunk;
            }
        }
        task.decorating = entries_.at(at)->terrain.get();
        busy_.insert(at);
        decorating_.insert(at);
        return task;
    }

    std::optional<std::int64_t> resolve;
    for (auto& [at, request] : requests_) {
        if (request.resolved || busy_.contains(at) || persisting_.contains(at)) {
            continue;
        }
        if (entries_.contains(at)) {
            request.resolved = true;
            wantsStale_ = true;
            continue;
        }
        if (!resolve.has_value() || request.priority < requests_.at(*resolve).priority) {
            resolve = at;
        }
    }
    if (resolve.has_value()) {
        busy_.insert(*resolve);
        return Task{.kind = Task::Kind::Resolve, .key = *resolve};
    }

    refreshWants();
    std::optional<std::int64_t> terrain;
    std::int32_t best = 0;
    for (const auto& [at, priority] : terrainWants_) {
        if (busy_.contains(at) || persisting_.contains(at) || entries_.contains(at)) {
            continue;
        }
        if (!terrain.has_value() || priority < best ||
            (priority == best && std::pair{xOf(at), zOf(at)} < std::pair{xOf(*terrain), zOf(*terrain)})) {
            terrain = at;
            best = priority;
        }
    }
    if (terrain.has_value()) {
        busy_.insert(*terrain);
        return Task{.kind = Task::Kind::Terrain, .key = *terrain};
    }
    return std::nullopt;
}

void ProtoChunks::runTask(Task& task) {
    const std::int32_t chunkX = xOf(task.key);
    const std::int32_t chunkZ = zOf(task.key);
    switch (task.kind) {
    case Task::Kind::Resolve:
    case Task::Kind::Terrain:
        if (hooks_.load) {
            task.stored = hooks_.load(chunkX, chunkZ);
        }
        if (task.stored.has_value() && task.stored->stage == ChunkStage::Full) {
            if (task.kind == Task::Kind::Resolve && hooks_.finish) {
                hooks_.finish(task.stored->chunk, false);
            }
        } else if (task.stored.has_value()) {
            task.made =
                std::make_unique<realTerrainGenerator::GeneratedTerrain>(hooks_.adopt(std::move(task.stored->chunk)));
            task.made->chunk().setStage(task.stored->stage);
        } else if (task.kind == Task::Kind::Terrain) {
            task.made = std::make_unique<realTerrainGenerator::GeneratedTerrain>(hooks_.terrain(chunkX, chunkZ));
            task.made->chunk().setStage(ChunkStage::Terrain);
        }
        break;
    case Task::Kind::Decorate: {
        ProtoRegion region{chunkX, chunkZ, task.region};
        hooks_.decorate(*task.decorating, region);
        task.decorating->chunk().setStage(ChunkStage::Decorated);
        break;
    }
    case Task::Kind::Finish:
        task.finishing->setStage(ChunkStage::Full);
        if (hooks_.finish) {
            hooks_.finish(*task.finishing, true);
        }
        break;
    }
}

bool ProtoChunks::step() {
    std::optional<Task> task;
    {
        const std::lock_guard lock{mutex_};
        task = pick();
    }
    if (!task.has_value()) {
        return false;
    }
    runTask(*task);
    std::vector<Chunk> leaving;
    {
        const std::lock_guard lock{mutex_};
        busy_.erase(task->key);
        wantsStale_ = true;
        const auto request = requests_.find(task->key);
        switch (task->kind) {
        case Task::Kind::Resolve:
            if (task->stored.has_value() && task->stored->stage == ChunkStage::Full) {
                full_.insert(task->key);
                finished_.push_back(Finished{.key = task->key,
                                             .chunk = std::move(task->stored->chunk),
                                             .waited = request != requests_.end() && request->second.waited});
                if (request != requests_.end()) {
                    requests_.erase(request);
                }
                break;
            }
            full_.erase(task->key);
            if (task->made != nullptr) {
                entries_.emplace(task->key, std::make_unique<Entry>(Entry{.terrain = std::move(task->made),
                                                                          .stage = task->stored->stage,
                                                                          .used = ++clock_}));
            }
            if (request != requests_.end()) {
                request->second.resolved = true;
            }
            break;
        case Task::Kind::Terrain:
            if (task->made == nullptr) {
                full_.insert(task->key);
            } else {
                const ChunkStage stage = task->made->chunk().stage();
                entries_.emplace(task->key, std::make_unique<Entry>(Entry{
                                                .terrain = std::move(task->made), .stage = stage, .used = ++clock_}));
            }
            break;
        case Task::Kind::Decorate:
            decorating_.erase(task->key);
            entries_.at(task->key)->stage = ChunkStage::Decorated;
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                for (std::int32_t dz = -1; dz <= 1; ++dz) {
                    if (const auto neighbour = entries_.find(key(xOf(task->key) + dx, zOf(task->key) + dz));
                        neighbour != entries_.end()) {
                        neighbour->second->touched = true;
                    }
                }
            }
            break;
        case Task::Kind::Finish:
            finished_.push_back(Finished{.key = task->key,
                                         .chunk = std::move(*task->finishing),
                                         .waited = request != requests_.end() && request->second.waited});
            if (request != requests_.end()) {
                requests_.erase(request);
            }
            break;
        }
        leaving = evict();
    }
    persistOutside(std::move(leaving));
    return true;
}

std::vector<Chunk> ProtoChunks::evict() {
    if (entries_.size() <= capacity_) {
        return {};
    }
    refreshWants();
    std::unordered_set<std::int64_t> needed;
    const auto around = [&needed](const std::int64_t at) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                needed.insert(key(xOf(at) + dx, zOf(at) + dz));
            }
        }
    };
    for (const auto& [at, request] : requests_) {
        around(at);
    }
    for (const auto& [at, priority] : decorationWants_) {
        around(at);
    }
    for (const std::int64_t at : decorating_) {
        around(at);
    }
    std::vector<std::pair<std::uint64_t, std::int64_t>> spare;
    for (const auto& [at, entry] : entries_) {
        if (busy_.contains(at) || needed.contains(at)) {
            continue;
        }
        spare.emplace_back(entry->used, at);
    }
    std::ranges::sort(spare);
    const std::size_t target = capacity_ - (capacity_ / 8);
    std::vector<Chunk> leaving;
    for (const auto& [used, at] : spare) {
        if (entries_.size() <= target) {
            break;
        }
        const auto entry = entries_.find(at);
        if (entry->second->stage == ChunkStage::Decorated || entry->second->touched) {
            Chunk chunk = std::move(entry->second->terrain->chunk());
            chunk.setStage(entry->second->stage);
            leaving.push_back(std::move(chunk));
            persisting_.insert(at);
        }
        entries_.erase(entry);
    }
    wantsStale_ = true;
    return leaving;
}

void ProtoChunks::persistOutside(std::vector<Chunk> chunks) {
    if (chunks.empty()) {
        return;
    }
    std::vector<std::int64_t> keys;
    keys.reserve(chunks.size());
    for (const Chunk& chunk : chunks) {
        keys.push_back(key(chunk.chunkX(), chunk.chunkZ()));
    }
    if (hooks_.persist) {
        hooks_.persist(std::move(chunks));
    }
    const std::lock_guard lock{mutex_};
    for (const std::int64_t at : keys) {
        persisting_.erase(at);
    }
}

void ProtoChunks::persistAll() {
    std::vector<Chunk> leaving;
    {
        const std::lock_guard lock{mutex_};
        for (auto at = entries_.begin(); at != entries_.end();) {
            Entry& entry = *at->second;
            if (busy_.contains(at->first) || (entry.stage != ChunkStage::Decorated && !entry.touched)) {
                ++at;
                continue;
            }
            Chunk chunk = std::move(entry.terrain->chunk());
            chunk.setStage(entry.stage);
            leaving.push_back(std::move(chunk));
            persisting_.insert(at->first);
            at = entries_.erase(at);
        }
        wantsStale_ = true;
    }
    persistOutside(std::move(leaving));
}

std::vector<Chunk> ProtoChunks::takeFinished() {
    const std::lock_guard lock{mutex_};
    std::vector<Chunk> taken;
    for (auto at = finished_.begin(); at != finished_.end();) {
        if (at->waited) {
            ++at;
            continue;
        }
        taken.push_back(std::move(at->chunk));
        at = finished_.erase(at);
    }
    return taken;
}

std::optional<Chunk> ProtoChunks::generateNow(const std::int32_t chunkX, const std::int32_t chunkZ) {
    const std::int64_t at = key(chunkX, chunkZ);
    {
        const std::lock_guard lock{mutex_};
        if (full_.contains(at) && !requests_.contains(at) &&
            std::ranges::none_of(finished_, [at](const Finished& done) { return done.key == at; })) {
            return std::nullopt;
        }
        Request& request = requests_[at];
        request.priority = kImmediate;
        request.waited = true;
        for (Finished& done : finished_) {
            if (done.key == at) {
                done.waited = true;
            }
        }
        wantsStale_ = true;
    }
    while (true) {
        {
            const std::lock_guard lock{mutex_};
            const auto done =
                std::ranges::find_if(finished_, [at](const Finished& finished) { return finished.key == at; });
            if (done != finished_.end()) {
                Chunk chunk = std::move(done->chunk);
                finished_.erase(done);
                return chunk;
            }
            if (!requests_.contains(at)) {
                return std::nullopt;
            }
        }
        if (!step()) {
            std::this_thread::sleep_for(kWaitPoll);
        }
    }
}

} // namespace basalt::server::world::terrain
