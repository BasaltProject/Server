#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace basalt::server::registry {
namespace {
using wire::common::Identifier;
using wire::protocol::play::ParticleOptionShape;

[[nodiscard]] Identifier vanilla(std::string_view path) { return Identifier::vanilla(std::string{path}); }
} // namespace

namespace items {
const Registry<Item>& registry() {
    static const Registry<Item> built = [] {
        Registry<Item> registry;
        for (const auto& [protocolId, path] : itemIdsTable()) {
            Identifier id = vanilla(path);
            registry.registerEntry(protocolId, id, Item{id, protocolId});
        }
        return registry;
    }();
    return built;
}
} // namespace items

namespace entityTypes {
namespace {
struct Table {
    std::vector<EntityTypeEntry> byProtocolId;
    std::unordered_map<Identifier, std::size_t> byName;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        table.byProtocolId.reserve(entityTypeIdsTable().size());
        for (const auto& [protocolId, path] : entityTypeIdsTable()) {
            table.byProtocolId.push_back({vanilla(path), protocolId});
        }
        std::ranges::sort(table.byProtocolId, {}, &EntityTypeEntry::protocolId);
        for (std::size_t index = 0; index < table.byProtocolId.size(); ++index) {
            table.byName.emplace(table.byProtocolId[index].id, index);
        }
        return table;
    }();
    return built;
}
} // namespace

std::span<const EntityTypeEntry> all() { return table().byProtocolId; }

const EntityTypeEntry* of(const Identifier& id) {
    const auto found = table().byName.find(id);
    return found == table().byName.end() ? nullptr : &table().byProtocolId[found->second];
}

const EntityTypeEntry* byProtocolId(std::int32_t protocolId) {
    const auto found = std::ranges::lower_bound(table().byProtocolId, protocolId, {}, &EntityTypeEntry::protocolId);
    if (found == table().byProtocolId.end() || found->protocolId != protocolId) {
        return nullptr;
    }
    return &*found;
}

const Identifier& player() {
    static const Identifier id = Identifier::vanilla("player");
    return id;
}
} // namespace entityTypes

namespace particleTypes {
namespace {
// parity: `ParticleTypes`' own registrations.
constexpr std::array<std::pair<std::string_view, ParticleOptionShape>, 23> kShapes = {{
    {"block", ParticleOptionShape::BlockState},
    {"block_marker", ParticleOptionShape::BlockState},
    {"falling_dust", ParticleOptionShape::BlockState},
    {"dust_pillar", ParticleOptionShape::BlockState},
    {"block_crumble", ParticleOptionShape::BlockState},
    {"entity_effect", ParticleOptionShape::Color},
    {"flash", ParticleOptionShape::Color},
    {"tinted_leaves", ParticleOptionShape::Color},
    {"effect", ParticleOptionShape::Spell},
    {"instant_effect", ParticleOptionShape::Spell},
    {"dust", ParticleOptionShape::Dust},
    {"dust_color_transition", ParticleOptionShape::DustTransition},
    {"item", ParticleOptionShape::Item},
    {"trail", ParticleOptionShape::Trail},
    {"sculk_charge", ParticleOptionShape::Number},
    {"dragon_breath", ParticleOptionShape::Number},
    {"shriek", ParticleOptionShape::Delay},
    {"geyser", ParticleOptionShape::Geyser},
    {"geyser_plume", ParticleOptionShape::Geyser},
    {"geyser_base", ParticleOptionShape::GeyserBase},
    {"geyser_poof", ParticleOptionShape::GeyserBase},
    {"vibration", ParticleOptionShape::Vibration},
    {"", ParticleOptionShape::None},
}};

struct Table {
    std::vector<ParticleType> byProtocolId;
    std::unordered_map<Identifier, std::size_t> byName;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        table.byProtocolId.reserve(particleTypeIdsTable().size());
        for (const auto& [protocolId, path] : particleTypeIdsTable()) {
            const auto shape =
                std::ranges::find(kShapes, path, &std::pair<std::string_view, ParticleOptionShape>::first);
            table.byProtocolId.push_back(
                {vanilla(path), protocolId, shape == kShapes.end() ? ParticleOptionShape::None : shape->second});
        }
        std::ranges::sort(table.byProtocolId, {}, &ParticleType::protocolId);
        for (std::size_t index = 0; index < table.byProtocolId.size(); ++index) {
            table.byName.emplace(table.byProtocolId[index].id, index);
        }
        return table;
    }();
    return built;
}
} // namespace

std::span<const ParticleType> all() { return table().byProtocolId; }

const ParticleType* of(const Identifier& id) {
    const auto found = table().byName.find(id);
    return found == table().byName.end() ? nullptr : &table().byProtocolId[found->second];
}
} // namespace particleTypes

namespace dataComponents {
namespace {
struct Table {
    std::unordered_map<Identifier, std::int32_t> byName;
    std::unordered_map<std::int32_t, Identifier> byId;
    std::vector<Identifier> names;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        for (const auto& [id, path] : dataComponentIdsTable()) {
            Identifier name = vanilla(path);
            table.byName.emplace(name, id);
            table.byId.emplace(id, name);
            table.names.push_back(std::move(name));
        }
        return table;
    }();
    return built;
}
} // namespace

std::optional<std::int32_t> idOf(const Identifier& name) {
    const auto found = table().byName.find(name);
    return found == table().byName.end() ? std::nullopt : std::optional{found->second};
}

const Identifier* nameOf(std::int32_t id) {
    const auto found = table().byId.find(id);
    return found == table().byId.end() ? nullptr : &found->second;
}

const std::vector<Identifier>& all() { return table().names; }
} // namespace dataComponents

namespace blockEntityTypes {
namespace {
struct Table {
    std::unordered_map<std::string_view, std::int32_t> idsByName;
    std::unordered_map<Identifier, Identifier> typesByBlock;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        std::int32_t id = 0;
        for (const std::string_view name : blockEntityTypeNamesTable()) {
            table.idsByName.emplace(name, id++);
        }
        for (const auto& [block, type] : blockEntityOwnersTable()) {
            table.typesByBlock.emplace(vanilla(block), vanilla(type));
        }
        return table;
    }();
    return built;
}
} // namespace

std::optional<std::int32_t> idOf(const Identifier& identifier) {
    if (identifier.namespaceName() != Identifier::kVanillaNamespace) {
        return std::nullopt;
    }
    const auto found = table().idsByName.find(identifier.path());
    return found == table().idsByName.end() ? std::nullopt : std::optional{found->second};
}

const Identifier* ofBlock(const Identifier& block) {
    const auto found = table().typesByBlock.find(block);
    return found == table().typesByBlock.end() ? nullptr : &found->second;
}

std::size_t size() { return blockEntityTypeNamesTable().size(); }
} // namespace blockEntityTypes
} // namespace basalt::server::registry
