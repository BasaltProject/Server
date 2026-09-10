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

/// The block a wall-mounted one drops as, or nothing when it is not one of them. parity:
/// `Blocks.wallVariant` gives it the standing block's loot table, so the jar carries no
/// `blocks/redstone_wall_torch.json` at all. Dropping the `wall_` segment is the whole mapping.
[[nodiscard]] std::optional<std::string> standingForm(const Identifier& block) {
    if (block.namespaceName() != Identifier::kVanillaNamespace) {
        return std::nullopt;
    }
    const std::string& path = block.path();
    const std::size_t at = path.starts_with("wall_") ? 0 : path.find("_wall_");
    if (at == std::string::npos) {
        return std::nullopt;
    }
    std::string standing = path;
    standing.erase(at == 0 ? 0 : at + 1, std::string_view{"wall_"}.size());
    return standing;
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
    const std::optional<std::string> standing = standingForm(block);
    const std::string& path = standing.value_or(block.path());
    return read(standing.has_value() ? Identifier::vanilla(path) : block,
                std::string{kPrefix} + "blocks/" + path + std::string{kJsonSuffix}, "drops nothing");
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
