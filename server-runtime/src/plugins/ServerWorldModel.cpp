#include "DataFix.hpp"
#include "ServerWorldRefs.hpp"
#include "WorldBlockUpdates.hpp"
#include "WorldItemUse.hpp"

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/DroppedItemOps.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/PlayerEntityOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/server/runtime/world/poi/PoiTypes.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace basalt::server::runtime::plugins {
namespace reg = server::registry;

using server::plugins::PluginEntityRef;
using server::plugins::PluginWorldRef;
using wire::common::Identifier;
using worldModel::LoadedWorldRef;

std::vector<std::shared_ptr<PluginWorldRef>> LoadedWorlds::all() const {
    std::vector<std::shared_ptr<PluginWorldRef>> refs;
    if (!hooks_->allWorlds) {
        return refs;
    }
    auto worlds = hooks_->allWorlds();
    if (hooks_->defaultWorldName) {
        const std::string primary = hooks_->defaultWorldName();
        std::stable_partition(worlds.begin(), worlds.end(),
                              [&primary](const auto* world) { return world->name() == primary; });
    }
    for (world::LoadedWorld* world : worlds) {
        refs.push_back(std::make_shared<LoadedWorldRef>(*world, hooks_, players_));
    }
    return refs;
}

std::shared_ptr<PluginWorldRef> LoadedWorlds::byName(std::string_view name) const {
    world::LoadedWorld* world = hooks_->worldFor ? hooks_->worldFor(std::string{name}) : nullptr;
    return world == nullptr ? nullptr : std::make_shared<LoadedWorldRef>(*world, hooks_, players_);
}

std::shared_ptr<PluginWorldRef> LoadedWorlds::defaultWorld() const {
    return hooks_->defaultWorldName ? byName(hooks_->defaultWorldName()) : nullptr;
}

std::shared_ptr<PluginWorldRef> LoadedWorlds::create(std::string_view name, std::string_view dimension,
                                                     std::string_view type, std::optional<std::int64_t> seed) {
    // Named and refused rather than guessed: a typo that quietly made an overworld is a world to delete by hand.
    std::string dimensionName{dimension};
    if (dimensionName.starts_with("minecraft:")) {
        dimensionName = dimensionName.substr(std::string_view{"minecraft:"}.size());
    }
    const std::optional<config::WorldDimension> theDimension = config::dimensionByName(dimensionName);
    const std::optional<config::WorldType> theType = config::worldTypeByName(type);
    if (!theDimension.has_value() || !theType.has_value() || !hooks_->makeWorld) {
        return nullptr;
    }
    world::LoadedWorld* made = hooks_->makeWorld(std::string{name}, *theDimension, *theType, seed);
    return made == nullptr ? nullptr : std::make_shared<LoadedWorldRef>(*made, hooks_, players_);
}

bool LoadedWorlds::unload(std::string_view name, bool save) {
    return hooks_->unloadWorld && hooks_->unloadWorld(std::string{name}, save);
}

bool LoadedWorlds::erase(std::string_view name) {
    return hooks_->deleteWorld && hooks_->deleteWorld(std::string{name});
}

std::int32_t LoadedWorlds::stateId(std::string_view text) const {
    return LoadedWorldRef::parseState(text).value_or(-1);
}

std::string LoadedWorlds::stateText(std::int32_t stateId) const {
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return {};
    }
    std::string text = block->toString();
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(stateId);
    char separator = '[';
    for (const auto& [name, value] : properties) {
        text += separator;
        text += name + "=" + value;
        separator = ',';
    }
    return properties.empty() ? text : text + "]";
}

std::vector<std::string> LoadedWorlds::blockTable() const {
    std::vector<std::string> table;
    const reg::Registry<reg::BlockState>& blocks = reg::blocks::registry();
    for (const std::int32_t defaultState : blocks.ids()) {
        const Identifier* name = blocks.nameOf(defaultState);
        const reg::blockStateDefinitions::Definition* definition = reg::blockStateDefinitions::of(*name);
        table.push_back(name->toString());
        table.push_back(std::to_string(definition == nullptr ? defaultState : definition->firstStateId()));
        table.push_back(std::to_string(definition == nullptr ? 1 : definition->stateCount()));
    }
    return table;
}

std::vector<std::string> LoadedWorlds::blockProperties(std::string_view block) const {
    std::vector<std::string> flat;
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{block});
    const reg::blockStateDefinitions::Definition* definition =
        id.has_value() ? reg::blockStateDefinitions::of(*id) : nullptr;
    if (definition == nullptr) {
        return flat;
    }
    for (const std::string& name : definition->propertyNames()) {
        std::string joined;
        for (const std::string& value : definition->valuesOf(name)) {
            joined += joined.empty() ? value : "|" + value;
        }
        flat.push_back(name);
        flat.push_back(joined);
    }
    return flat;
}

std::int32_t LoadedWorlds::stateFlags(std::int32_t stateId) const {
    std::int32_t bits = 0;
    bits |= reg::blockShapes::blocksMotion(stateId) ? 1 : 0;
    bits |= reg::blockShapes::isSolidRender(stateId) ? 2 : 0;
    bits |= reg::blockShapes::isFullCube(stateId) ? 4 : 0;
    bits |= reg::blockShapes::isReplaceable(stateId) ? 8 : 0;
    bits |= reg::blockFluids::hasFluid(stateId) ? 16 : 0;
    bits |= reg::blockShapes::isEmptyShape(stateId) ? 0 : 32;
    bits |= reg::blockSupport::conductsRedstone(stateId) ? 64 : 0;
    bits |= reg::blockSupport::ticksRandomly(stateId) ? 128 : 0;
    bits |= reg::blocks::isAir(stateId) ? 256 : 0;
    bits |= server::world::light::blockLight::emission(stateId) << 9;
    bits |= server::world::light::skyLight::opacity(stateId) << 13;
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    bits |= block != nullptr && reg::blockEntityTypes::ofBlock(*block) != nullptr ? (1 << 18) : 0;
    return bits;
}

std::vector<double> LoadedWorlds::blockHardness(std::string_view block) const {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{block});
    const reg::BlockHardness* found = id.has_value() ? reg::blocks::hardnessOf(*id) : nullptr;
    if (found == nullptr) {
        return {};
    }
    return {static_cast<double>(found->destroyTime), static_cast<double>(found->explosionResistance),
            found->requiresCorrectTool ? 1.0 : 0.0};
}

std::vector<double> LoadedWorlds::blockInfo(std::int32_t stateId) const {
    if (reg::blockStateDefinitions::blockOf(stateId) == nullptr) {
        return {};
    }
    const std::uint32_t color =
        reg::mapColors::argbOf(reg::mapColors::packed(reg::mapColors::of(stateId), reg::mapColors::Brightness::High));
    return {static_cast<double>(reg::blockMovement::frictionOf(stateId)),
            static_cast<double>(reg::blockMovement::speedFactorOf(stateId)),
            static_cast<double>(reg::blockMovement::jumpFactorOf(stateId)),
            static_cast<double>(static_cast<std::int32_t>(reg::blockSupport::pushReaction(stateId))),
            static_cast<double>(color & 0xFFFFFFU),
            static_cast<double>(reg::fireOdds::igniteOdds(stateId)),
            static_cast<double>(reg::fireOdds::burnOdds(stateId))};
}

std::vector<std::string> LoadedWorlds::gameRuleSpecs() const {
    std::vector<std::string> specs;
    for (const world::GameRule& rule : world::gameRules::all()) {
        specs.emplace_back(rule.id);
        specs.emplace_back(rule.kind == world::GameRule::Kind::Boolean ? "boolean" : "int");
        specs.push_back(rule.kind == world::GameRule::Kind::Boolean ? (rule.defaultFlag ? "true" : "false")
                                                                    : std::to_string(rule.defaultNumber));
    }
    return specs;
}

std::int32_t LoadedWorlds::gameEventFrequency(std::string_view event) const {
    return world::vibrations::frequencyOf(event);
}

std::string LoadedWorlds::poiTypeOf(std::int32_t stateId) const {
    const world::poi::PoiType type = world::poi::poiTypes::forState(stateId);
    return type == world::poi::PoiType::kNone ? std::string{} : std::string{world::poi::poiTypes::name(type)};
}

std::int32_t LoadedWorlds::poiTickets(std::string_view name) const {
    const std::optional<world::poi::PoiType> type = world::poi::poiTypes::byName(name);
    return type.has_value() ? world::poi::poiTypes::maxTickets(*type) : 0;
}

std::int32_t LoadedWorlds::nextEntityId() { return hooks_->nextEntityId ? hooks_->nextEntityId() : -1; }

std::string LoadedWorlds::blockInstrument(std::int32_t stateId) const {
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{reg::noteBlockInstruments::of(*block).serializedName()};
}

std::vector<std::string> LoadedWorlds::blockSounds(std::int32_t stateId) const {
    if (reg::blockStateDefinitions::blockOf(stateId) == nullptr) {
        return {};
    }
    const reg::BlockSoundTypeEntry& type = reg::blockSounds::of(stateId);
    return {std::to_string(type.volume),
            std::to_string(type.pitch),
            "minecraft:" + std::string{type.breakSound},
            "minecraft:" + std::string{type.stepSound},
            "minecraft:" + std::string{type.placeSound},
            "minecraft:" + std::string{type.hitSound},
            "minecraft:" + std::string{type.fallSound}};
}

std::int32_t LoadedWorlds::transformState(std::int32_t stateId, std::int32_t rotation, std::int32_t mirror) const {
    if (reg::blockStateDefinitions::blockOf(stateId) == nullptr) {
        return stateId;
    }
    return server::world::structure::StructureTemplate::transformedStateId(
        stateId, reg::blockStateDefinitions::propertiesOf(stateId),
        static_cast<server::world::structure::StructureMirror>(std::clamp(mirror, 0, 2)),
        static_cast<server::world::structure::StructureRotation>(std::clamp(rotation, 0, 3)));
}

std::vector<double> LoadedWorlds::toolFacts(std::string_view item, std::int32_t stateId) const {
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return {};
    }
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(std::string{item});
    const Identifier* tool = item.empty() || !parsed.has_value() ? nullptr : &*parsed;
    const world::realBlockTags::TagMembers* tags = hooks_->blockTags;
    const reg::TagContains contains = [tags](std::string_view tag, const Identifier& member) {
        return tags != nullptr && world::realBlockTags::contains(*tags, tag, member);
    };
    return {reg::itemTools::isCorrectForDrops(tool, *block, contains) ? 1.0 : 0.0,
            static_cast<double>(reg::itemTools::miningSpeed(tool, *block, contains))};
}

std::vector<std::int32_t> LoadedWorlds::collisionBoxes(std::int32_t stateId) const {
    if (reg::blockStateDefinitions::blockOf(stateId) == nullptr) {
        return {};
    }
    const std::span<const std::int32_t> boxes = reg::blockShapes::collisionBoxes(stateId);
    return {boxes.begin(), boxes.end()};
}

std::vector<std::string> LoadedWorlds::blockEntityTypes() const {
    std::vector<std::string> pairs;
    const reg::Registry<reg::BlockState>& blocks = reg::blocks::registry();
    for (const std::int32_t defaultState : blocks.ids()) {
        const Identifier* name = blocks.nameOf(defaultState);
        if (const Identifier* type = reg::blockEntityTypes::ofBlock(*name); type != nullptr) {
            pairs.push_back(name->toString());
            pairs.push_back(type->toString());
        }
    }
    return pairs;
}

std::int32_t LoadedWorlds::dataVersion() const { return dataFix::currentVersion(); }

std::optional<std::vector<std::uint8_t>> LoadedWorlds::fixNbt(std::string_view kind, std::span<const std::uint8_t> nbt,
                                                              std::int32_t fromVersion) const {
    const auto parsed = wire::nbt::nbtIO::read(nbt);
    const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
    if (given == nullptr) {
        return std::nullopt;
    }
    std::optional<wire::nbt::NbtCompound> fixed = dataFix::fixDocument(kind, *given, fromVersion);
    if (!fixed.has_value()) {
        return std::nullopt;
    }
    return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(*fixed)));
}

std::optional<std::string> LoadedWorlds::fixName(std::string_view kind, std::string_view text,
                                                 std::int32_t fromVersion) const {
    return dataFix::fixName(kind, text, fromVersion);
}

std::vector<std::string> LoadedWorlds::biomeIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, name] : reg::biomeIdsTable()) {
        ids.push_back("minecraft:" + std::string{name});
    }
    return ids;
}

std::vector<std::string> LoadedWorlds::tags(std::string_view registry) const {
    return hooks_->tags ? hooks_->tags(registry) : std::vector<std::string>{};
}

std::vector<std::string> LoadedWorlds::itemIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, name] : reg::itemIdsTable()) {
        ids.push_back("minecraft:" + std::string{name});
    }
    return ids;
}

std::vector<std::string> LoadedWorlds::registryIds(std::string_view registry) const {
    const auto tableIds = [](auto table) {
        std::vector<std::string> ids;
        for (const auto& [id, name] : table) {
            ids.push_back("minecraft:" + std::string{name});
        }
        return ids;
    };
    if (registry == "item") {
        return itemIds();
    }
    if (registry == "entity_type") {
        return entityTypeIds();
    }
    if (registry == "worldgen/biome") {
        return biomeIds();
    }
    if (registry == "particle_type") {
        return tableIds(reg::particleTypeIdsTable());
    }
    if (registry == "data_component_type") {
        return tableIds(reg::dataComponentIdsTable());
    }
    if (registry == "map_decoration_type") {
        std::vector<std::string> ids;
        for (std::int32_t id = 0; world::maps::decorationTypes::byId(id) != nullptr; ++id) {
            ids.push_back("minecraft:" + std::string{world::maps::decorationTypes::byId(id)->name});
        }
        return ids;
    }
    if (registry == "block_entity_type") {
        std::vector<std::string> ids;
        for (const std::string_view name : reg::blockEntityTypeNamesTable()) {
            ids.push_back("minecraft:" + std::string{name});
        }
        return ids;
    }
    if (registry == "mob_effect") {
        std::vector<std::string> ids;
        for (std::int32_t protocolId = 0; world::mobEffects::byProtocolId(protocolId).has_value(); ++protocolId) {
            ids.push_back(world::mobEffects::byProtocolId(protocolId)->toString());
        }
        return ids;
    }
    if (registry == "attribute") {
        std::vector<std::string> ids;
        for (const std::string_view name : reg::attributeIds::all()) {
            ids.push_back("minecraft:" + std::string{name});
        }
        return ids;
    }
    if (registry == "villager_profession") {
        std::vector<std::string> ids;
        for (const std::string_view name : entity::villagers::professions()) {
            ids.push_back("minecraft:" + std::string{name});
        }
        return ids;
    }
    if (registry == "villager_type") {
        return {"minecraft:desert", "minecraft:jungle", "minecraft:plains", "minecraft:savanna",
                "minecraft:snow",   "minecraft:swamp",  "minecraft:taiga"};
    }
    static const std::vector<wire::protocol::configuration::RegistryDataPacket> synced =
        registry::vanillaConfigurationRegistries::packets();
    for (const wire::protocol::configuration::RegistryDataPacket& packet : synced) {
        if (packet.registryId == "minecraft:" + std::string{registry}) {
            std::vector<std::string> ids;
            for (const wire::protocol::configuration::RegistryEntry& entry : packet.entries) {
                ids.push_back(entry.key);
            }
            return ids;
        }
    }
    return {};
}

std::optional<std::vector<std::uint8_t>> LoadedWorlds::registryData(std::string_view registry,
                                                                    std::string_view id) const {
    static const std::vector<wire::protocol::configuration::RegistryDataPacket> synced =
        registry::vanillaConfigurationRegistries::packets();
    for (const wire::protocol::configuration::RegistryDataPacket& packet : synced) {
        if (packet.registryId != "minecraft:" + std::string{registry}) {
            continue;
        }
        for (const wire::protocol::configuration::RegistryEntry& entry : packet.entries) {
            if (entry.key == id && entry.value.has_value()) {
                return wire::nbt::nbtIO::writeAnonymous(*entry.value);
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> LoadedWorlds::entityDefaultAttributes(std::string_view type) const {
    std::vector<std::string> out;
    const auto parsed = Identifier::parse(std::string{type});
    if (!parsed.has_value() || !reg::entityAttributes::isLiving(*parsed)) {
        return out;
    }
    for (const auto& [name, value] : reg::entityAttributes::of(*parsed)) {
        out.push_back("minecraft:" + name + "=" + std::to_string(value));
    }
    return out;
}

std::vector<std::string> LoadedWorlds::entityTypeIds() const {
    std::vector<std::string> ids;
    for (const reg::EntityTypeEntry& entry : reg::entityTypes::all()) {
        ids.push_back(entry.id.toString());
    }
    return ids;
}

server::plugins::PluginHandle RegistryPlaceholders::registerPlaceholder(std::string_view pluginId,
                                                                        std::string_view name, Value value) {
    const std::string identifier = std::string{pluginId} + "_" + std::string{name};
    server::plugins::PluginPlayers* players = players_;
    registry_->registerProvider(
        identifier, [players, value](std::string_view, const wire::common::placeholder::PlaceholderContext& context) {
            const player::PlayerSession* session = context.get(placeholder::playerSlot());
            const std::shared_ptr<server::plugins::PluginPlayerRef> player =
                session == nullptr ? nullptr : players->byName(session->name());
            return wire::common::placeholder::PlaceholderValue::of(value(player.get()));
        });
    wire::common::placeholder::PlaceholderRegistry* registry = registry_;
    return server::plugins::PluginHandle{[registry, identifier] { registry->unregisterProvider(identifier); }};
}

std::string RegistryPlaceholders::fill(std::string_view text, server::plugins::PluginPlayerRef* player) const {
    const player::PlayerSession* session = nullptr;
    if (player != nullptr && sessions_) {
        const std::string name = player->name();
        for (const player::PlayerSession* candidate : sessions_()) {
            if (candidate->name() == name) {
                session = candidate;
                break;
            }
        }
    }
    const wire::common::placeholder::PlaceholderContext context =
        wire::common::placeholder::PlaceholderContext{}.with(placeholder::playerSlot(), session);
    return wire::common::placeholder::substitute(text, *registry_, context);
}

std::optional<std::string> DatabasePluginStorage::playerValue(std::string_view pluginId, std::string_view uuid,
                                                              std::string_view key) {
    storage::PlayerStore* store = store_();
    return store == nullptr || !store->isOpen() ? std::nullopt : store->pluginData().get(pluginId, uuid, key);
}

void DatabasePluginStorage::setPlayerValue(std::string_view pluginId, std::string_view uuid, std::string_view key,
                                           const std::optional<std::string>& value) {
    storage::PlayerStore* store = store_();
    if (store != nullptr && store->isOpen()) {
        store->pluginData().set(pluginId, uuid, key, value);
    }
}

std::map<std::string, std::string> DatabasePluginStorage::playerValues(std::string_view pluginId,
                                                                       std::string_view uuid) {
    storage::PlayerStore* store = store_();
    if (store == nullptr || !store->isOpen()) {
        return {};
    }
    const std::map<std::string, std::string, std::less<>> stored = store->pluginData().all(pluginId, uuid);
    return std::map<std::string, std::string>{stored.begin(), stored.end()};
}

std::vector<std::pair<std::string, std::string>> DatabasePluginStorage::top(std::string_view pluginId,
                                                                            std::string_view key, std::int32_t limit) {
    storage::PlayerStore* store = store_();
    return store == nullptr || !store->isOpen() ? std::vector<std::pair<std::string, std::string>>{}
                                                : store->pluginData().top(pluginId, key, limit);
}

std::map<std::string, std::int64_t> DatabasePluginStorage::namespaces() {
    storage::PlayerStore* store = store_();
    if (store == nullptr || !store->isOpen()) {
        return {};
    }
    const std::map<std::string, std::int64_t, std::less<>> counted = store->pluginData().namespaces();
    return std::map<std::string, std::int64_t>{counted.begin(), counted.end()};
}

std::optional<std::vector<std::uint8_t>> DatabasePluginStorage::holderData(std::string_view kind, std::string_view id) {
    storage::PlayerStore* store = store_();
    return store == nullptr || !store->isOpen() ? std::nullopt : store->pluginData().holder(kind, id);
}

void DatabasePluginStorage::setHolderData(std::string_view kind, std::string_view id,
                                          std::span<const std::uint8_t> data) {
    storage::PlayerStore* store = store_();
    if (store != nullptr && store->isOpen()) {
        store->pluginData().setHolder(kind, id, data);
    }
}

std::int64_t DatabasePluginStorage::purge(std::string_view pluginId) {
    storage::PlayerStore* store = store_();
    if (store == nullptr || !store->isOpen()) {
        return 0;
    }
    const std::map<std::string, std::int64_t, std::less<>> counted = store->pluginData().namespaces();
    const auto found = counted.find(pluginId);
    if (found == counted.end()) {
        return 0;
    }
    store->pluginData().purge(pluginId);
    return found->second;
}

} // namespace basalt::server::runtime::plugins
