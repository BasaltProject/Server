#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>

#include <expected>

namespace basalt::server::runtime::world {
namespace {

using server::world::loot::LootTable;
using wire::common::Identifier;

constexpr std::string_view kPrefix = "data/minecraft/loot_table/";
constexpr std::string_view kJsonSuffix = ".json";

[[nodiscard]] std::string stripped(const std::string& entry) {
    std::string path = entry;
    if (path.starts_with(kPrefix)) {
        path.erase(0, kPrefix.size());
    }
    if (path.ends_with(kJsonSuffix)) {
        path.resize(path.size() - kJsonSuffix.size());
    }
    return path;
}

} // namespace

const LootTable* RealLootTables::byId(const Identifier& id) {
    return read(id, std::string{kPrefix} + id.path() + std::string{kJsonSuffix}, "rolls nothing");
}

const LootTable* RealLootTables::forEntity(const Identifier& type) {
    return read(Identifier::vanilla("entities/" + type.path()),
                std::string{kPrefix} + "entities/" + type.path() + std::string{kJsonSuffix}, "rolls nothing");
}

const LootTable* RealLootTables::forBlock(const Identifier& block) {
    return read(block, std::string{kPrefix} + "blocks/" + block.path() + std::string{kJsonSuffix}, "drops nothing");
}

std::vector<Identifier> RealLootTables::ids() const {
    std::vector<Identifier> ids;
    if (!jar_.entriesUnder) {
        return ids;
    }
    for (const std::string& entry : jar_.entriesUnder(kPrefix)) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        ids.push_back(Identifier::vanilla(stripped(entry)));
    }
    return ids;
}

const LootTable* RealLootTables::read(const Identifier& key, const std::string& path, std::string_view verb) {
    {
        const std::lock_guard<std::mutex> guard(mutex_);
        if (const auto found = cache_.find(key); found != cache_.end()) {
            return found->second.has_value() ? &*found->second : nullptr;
        }
    }
    // No jar at all is not a miss worth remembering: the install may still be cold.
    if (!jar_.readText) {
        return nullptr;
    }
    std::optional<LootTable> resolved;
    const std::optional<std::string> json = jar_.readText(path);
    if (!json.has_value()) {
        logger_.debug([&] { return key.toString() + " " + std::string{verb} + ": no such loot table"; });
    } else if (std::expected<LootTable, server::world::loot::LootTableParseError> parsed =
                   server::world::loot::lootTableJson::parse(*json);
               parsed.has_value()) {
        resolved = std::move(*parsed);
    } else {
        logger_.debug([&] { return key.toString() + " " + std::string{verb} + ": " + parsed.error().message; });
    }
    const std::lock_guard<std::mutex> guard(mutex_);
    // try_emplace, not insert_or_assign: a thread that lost the race already handed its table out.
    const auto entry = cache_.try_emplace(key, std::move(resolved)).first;
    return entry->second.has_value() ? &*entry->second : nullptr;
}

} // namespace basalt::server::runtime::world
