#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/AreaEffectClouds.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Axolotls.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Breezes.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/FishingHooks.hpp>
#include <basalt/server/runtime/entity/Frogs.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/Goats.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace basalt::server::runtime::entity {
namespace {

/// parity: `LivingEntity.DATA_EFFECT_PARTICLES` and `DATA_EFFECT_AMBIENCE_ID`.
constexpr std::uint8_t kEffectParticlesDataIndex = 10;
constexpr std::uint8_t kEffectAmbienceDataIndex = 11;
/// parity: `LivingEntity.DATA_STINGER_COUNT_ID`, after the arrows stuck in it.
constexpr std::uint8_t kArrowCountDataIndex = 12;
/// parity: `Wolf`'s anger after the tamable's two and its own interest and collar.
constexpr std::uint8_t kWolfInterestedDataIndex = 20;
constexpr std::uint8_t kWolfAngerEndDataIndex = 22;
/// parity: `Enderman`'s creepy and stared-at flags after its carried block at 16.
constexpr std::uint8_t kEndermanCreepyDataIndex = 17;
constexpr std::uint8_t kEndermanStaredAtDataIndex = 18;
constexpr std::uint8_t kStingerCountDataIndex = 13;

/// parity: `ExperienceOrb.DATA_VALUE`.
constexpr std::uint8_t kOrbValueDataIndex = 8;

/// parity: `Pufferfish.PUFF_STATE`, the integer right after `AbstractFish`'s moving flag.
constexpr std::uint8_t kPufferfishPuffDataIndex = 17;

using wire::nbt::NbtCompound;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SetEquipmentPacket;
using Entry = SetEntityDataPacket::Entry;
using Entries = std::vector<Entry>;
using RegistryHolder = wire::protocol::play::RegistryHolder;

/// `AbstractCubeMob.ID_SIZE`: two `AgeableMob` fields follow the common index 16.
constexpr std::uint8_t kCubeSizeDataIndex = 18;
constexpr std::uint8_t kAbstractHorseFlagsDataIndex = 18;
constexpr std::uint8_t kTridentLoyaltyDataIndex = 11;
constexpr std::uint8_t kTridentFoilDataIndex = 12;
constexpr std::uint8_t kMobFlagsDataIndex = 15;
/// parity: `Fox.DATA_FLAGS_ID` after the type, and `Panda`'s counters and flags after the genes.
constexpr std::uint8_t kFoxFlagsDataIndex = 20;
constexpr std::uint8_t kPandaUnhappyDataIndex = 18;
constexpr std::uint8_t kPandaSneezeDataIndex = 19;
constexpr std::uint8_t kPandaEatDataIndex = 20;
constexpr std::uint8_t kPandaFlagsDataIndex = 23;
constexpr std::uint8_t kAirSupplyDataIndex = 1;
constexpr std::int32_t kDefaultAirSupply = 300;

using Ids = std::map<std::string, std::int32_t, std::less<>>;

const Ids& villagerTypeIds() {
    static const Ids kIds{{"minecraft:desert", 0},  {"minecraft:jungle", 1}, {"minecraft:plains", 2},
                          {"minecraft:savanna", 3}, {"minecraft:snow", 4},   {"minecraft:swamp", 5},
                          {"minecraft:taiga", 6}};
    return kIds;
}

const Ids& villagerProfessionIds() {
    static const Ids kIds{{"minecraft:none", 0},         {"minecraft:armorer", 1},    {"minecraft:butcher", 2},
                          {"minecraft:cartographer", 3}, {"minecraft:cleric", 4},     {"minecraft:farmer", 5},
                          {"minecraft:fisherman", 6},    {"minecraft:fletcher", 7},   {"minecraft:leatherworker", 8},
                          {"minecraft:librarian", 9},    {"minecraft:mason", 10},     {"minecraft:nitwit", 11},
                          {"minecraft:shepherd", 12},    {"minecraft:toolsmith", 13}, {"minecraft:weaponsmith", 14}};
    return kIds;
}

constexpr std::int32_t kSalmonMedium = 1;

const Ids& salmonVariants() {
    static const Ids kIds{{"small", 0}, {"medium", kSalmonMedium}, {"large", 2}};
    return kIds;
}

const Ids& pandaGeneIds() {
    static const Ids kIds{{"normal", 0}, {"lazy", 1}, {"worried", 2},   {"playful", 3},
                          {"brown", 4},  {"weak", 5}, {"aggressive", 6}};
    return kIds;
}

std::optional<std::int32_t> idOf(const Ids& ids, std::string_view name) {
    const auto found = ids.find(name);
    return found == ids.end() ? std::nullopt : std::optional{found->second};
}

/// parity: `Entity`'s eight fields.
constexpr std::uint8_t kPaintingVariantIndex = 9;

Ids syncedIds(const std::vector<std::string>& paths) {
    Ids ids;
    std::int32_t index = 0;
    for (const std::string& path : paths) {
        ids.emplace(path.find(':') == std::string::npos ? "minecraft:" + path : path, index++);
    }
    return ids;
}

/// parity: `defineSynchedData` starts on `fallback`.
void addRegistryMetadata(Entries& into, const NbtCompound& data, std::string_view key, std::uint8_t index,
                         const std::vector<std::string>& registry, std::string_view fallback, RegistryHolder holder) {
    const std::string_view name = data.getString(key);
    const std::optional<std::int32_t> id = idOf(syncedIds(registry), name);
    if (id.has_value() && name != fallback) {
        into.push_back(
            Entry{.index = index, .value = SetEntityDataPacket::RegistryIdValue{.value = *id, .registry = holder}});
    }
}

/// parity: `ItemFrame`'s item and rotation fields.
void addItemFrameMetadata(Entries& into, const NbtCompound& data) {
    if (const NbtCompound* held = data.getCompound("Item"); held != nullptr) {
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(std::string{held->getString("id")});
        const server::registry::Item* item = id.has_value() ? server::registry::items::registry().byName(*id) : nullptr;
        if (item != nullptr) {
            into.push_back(Entry{.index = SetEntityDataPacket::kItemFrameStackIndex,
                                 .value = SetEntityDataPacket::ItemValue{
                                     .stack = wire::protocol::play::WireItemStack{.itemId = item->protocolId,
                                                                                  .count = held->getInt("count", 1)}}});
        }
    }
    if (const std::int32_t rotation = data.getByte("ItemRotation", 0); rotation != 0) {
        into.push_back(Entry{.index = SetEntityDataPacket::kItemFrameStackIndex + 1,
                             .value = SetEntityDataPacket::IntValue{.value = rotation}});
    }
}

/// parity: `ResolvableProfile.STREAM_CODEC`, from the profile a mannequin saves.
[[nodiscard]] std::vector<std::uint8_t> profileBytes(const wire::nbt::NbtTag& saved) {
    wire::io::ByteBuffer out;
    std::string name;
    std::optional<wire::common::Uuid> id;
    std::vector<std::tuple<std::string, std::string, std::optional<std::string>>> properties;
    std::optional<std::string> texture;
    std::optional<std::string> cape;
    std::optional<std::string> elytra;
    std::optional<bool> slim;
    if (const std::string* only = saved.get<std::string>(); only != nullptr) {
        name = *only;
    } else if (const NbtCompound* profile = saved.get<NbtCompound>(); profile != nullptr) {
        name = std::string{profile->getString("name")};
        if (const std::span<const std::int32_t> parts = profile->getIntArray("id"); parts.size() == 4) {
            const auto word = [parts](std::size_t at) {
                return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
            };
            id = wire::common::Uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)};
        }
        if (const wire::nbt::NbtList* held = profile->getList("properties"); held != nullptr) {
            for (const wire::nbt::NbtTag& tag : held->elements()) {
                if (const NbtCompound* property = tag.get<NbtCompound>(); property != nullptr) {
                    properties.emplace_back(
                        std::string{property->getString("name")}, std::string{property->getString("value")},
                        property->contains("signature") ? std::optional{std::string{property->getString("signature")}}
                                                        : std::nullopt);
                }
            }
        }
        const auto identifier = [profile](std::string_view key) -> std::optional<std::string> {
            const std::string_view value = profile->getString(key);
            return value.empty() ? std::nullopt : std::optional{std::string{value}};
        };
        texture = identifier("texture");
        cape = identifier("cape");
        elytra = identifier("elytra");
        if (const std::string_view model = profile->getString("model"); !model.empty()) {
            slim = model == "slim";
        }
    }
    const bool full = !name.empty() && id.has_value();
    wire::protocol::writeBoolean(out, full);
    if (full) {
        wire::protocol::writeUuid(out, *id);
        wire::protocol::writeString(out, name);
    } else {
        wire::protocol::writeBoolean(out, !name.empty());
        if (!name.empty()) {
            wire::protocol::writeString(out, name);
        }
        wire::protocol::writeBoolean(out, id.has_value());
        if (id.has_value()) {
            wire::protocol::writeUuid(out, *id);
        }
    }
    wire::protocol::varInt::write(out, static_cast<std::int32_t>(properties.size()));
    for (const auto& [key, value, signature] : properties) {
        wire::protocol::writeString(out, key);
        wire::protocol::writeString(out, value);
        wire::protocol::writeBoolean(out, signature.has_value());
        if (signature.has_value()) {
            wire::protocol::writeString(out, *signature);
        }
    }
    for (const std::optional<std::string>& asset : {texture, cape, elytra}) {
        wire::protocol::writeBoolean(out, asset.has_value());
        if (asset.has_value()) {
            wire::protocol::writeString(out, *asset);
        }
    }
    wire::protocol::writeBoolean(out, slim.has_value());
    if (slim.has_value()) {
        wire::protocol::writeBoolean(out, *slim);
    }
    const auto bytes = out.readSpan(out.readableBytes());
    return {bytes.begin(), bytes.end()};
}

/// parity: `Mannequin`'s saved data, which the client draws the figure from.
void addMannequinMetadata(Entries& into, const NbtCompound& data) {
    constexpr std::uint8_t kMainHandIndex = 15;
    constexpr std::uint8_t kLayersIndex = 16;
    constexpr std::uint8_t kProfileIndex = 17;
    constexpr std::uint8_t kImmovableIndex = 18;
    constexpr std::uint8_t kDescriptionIndex = 19;
    constexpr std::int32_t kProfileSerializer = 41;
    static constexpr std::array<std::string_view, 7> kLayers{
        "cape", "jacket", "left_sleeve", "right_sleeve", "left_pants_leg", "right_pants_leg", "hat"};
    using wire::protocol::play::Pose;
    const std::string_view pose = data.getString("pose");
    const std::optional<Pose> saved = pose == "crouching"     ? std::optional{Pose::Crouching}
                                      : pose == "swimming"    ? std::optional{Pose::Swimming}
                                      : pose == "fall_flying" ? std::optional{Pose::FallFlying}
                                      : pose == "sleeping"    ? std::optional{Pose::Sleeping}
                                                              : std::nullopt;
    if (saved.has_value()) {
        into.push_back(
            Entry{.index = SetEntityDataPacket::kPoseIndex, .value = SetEntityDataPacket::PoseValue{*saved}});
    }
    if (data.getString("main_hand") == "left") {
        into.push_back(
            Entry{.index = kMainHandIndex,
                  .value = SetEntityDataPacket::RegistryIdValue{.value = 0, .registry = RegistryHolder::HumanoidArm}});
    }
    std::int32_t mask = 0x7F;
    if (const wire::nbt::NbtList* hidden = data.getList("hidden_layers"); hidden != nullptr) {
        for (const wire::nbt::NbtTag& tag : hidden->elements()) {
            const std::string* layer = tag.get<std::string>();
            for (std::size_t bit = 0; layer != nullptr && bit < kLayers.size(); ++bit) {
                if (*layer == kLayers[bit]) {
                    mask &= ~(1 << bit);
                }
            }
        }
    }
    into.push_back(
        Entry{.index = kLayersIndex, .value = SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(mask)}});
    if (const wire::nbt::NbtTag* profile = data.find("profile"); profile != nullptr) {
        into.push_back(Entry{
            .index = kProfileIndex,
            .value = SetEntityDataPacket::RawValue{.serializer = kProfileSerializer, .bytes = profileBytes(*profile)}});
    }
    if (data.getBoolean("immovable")) {
        into.push_back(Entry{.index = kImmovableIndex, .value = SetEntityDataPacket::BoolValue{.value = true}});
    }
    if (data.getBoolean("hide_description")) {
        into.push_back(
            Entry{.index = kDescriptionIndex, .value = SetEntityDataPacket::OptionalTextValue{.value = std::nullopt}});
    } else if (const wire::nbt::NbtTag* description = data.find("description"); description != nullptr) {
        into.push_back(Entry{.index = kDescriptionIndex,
                             .value = SetEntityDataPacket::OptionalTextValue{
                                 .value = wire::text::componentNbt::deserialize(*description)}});
    }
}

void addArmorStandMetadata(Entries& into, const NbtCompound& data) {
    std::int8_t flags = 0;
    if (data.getBoolean("Small")) {
        flags |= 1;
    }
    if (data.getBoolean("ShowArms")) {
        flags |= 4;
    }
    if (data.getBoolean("NoBasePlate")) {
        flags |= 8;
    }
    if (data.getBoolean("Marker")) {
        flags |= 16;
    }
    if (flags != 0) {
        into.push_back(Entry{.index = 15, .value = SetEntityDataPacket::ByteValue{.value = flags}});
    }
    const NbtCompound* pose = data.getCompound("Pose");
    if (pose == nullptr) {
        return;
    }
    static constexpr std::array<std::pair<std::string_view, std::uint8_t>, 6> kPoses{
        {{"Head", 16}, {"Body", 17}, {"LeftArm", 18}, {"RightArm", 19}, {"LeftLeg", 20}, {"RightLeg", 21}}};
    for (const auto& [name, index] : kPoses) {
        const wire::nbt::NbtList* values = pose->getList(name);
        if (values == nullptr || values->size() != 3) {
            continue;
        }
        const float* x = (*values)[0].get<float>();
        const float* y = (*values)[1].get<float>();
        const float* z = (*values)[2].get<float>();
        if (x == nullptr || y == nullptr || z == nullptr) {
            continue;
        }
        into.push_back(Entry{.index = index, .value = SetEntityDataPacket::RotationsValue{.x = *x, .y = *y, .z = *z}});
    }
}

void addCatMetadata(Entries& into, const NbtCompound& data) {
    const std::optional<std::int32_t> variant =
        idOf(syncedIds(runtime::registry::vanillaRegistryData::catVariants()), data.getString("variant"));
    if (variant.has_value() && data.getString("variant") != "minecraft:black") {
        into.push_back(Entry{
            .index = 20,
            .value = SetEntityDataPacket::RegistryIdValue{.value = *variant, .registry = RegistryHolder::CatVariant}});
    }
    const std::int32_t collarColor = data.getByte("CollarColor", 14) & 0xFF;
    if (collarColor != 14) {
        into.push_back(Entry{.index = 23, .value = SetEntityDataPacket::IntValue{.value = collarColor}});
    }
    addRegistryMetadata(into, data, "sound_variant", 24, runtime::registry::vanillaRegistryData::catSoundVariants(),
                        "minecraft:classic", RegistryHolder::CatSoundVariant);
}

void addVillagerMetadata(Entries& into, const NbtCompound& data, std::uint8_t dataIndex, std::uint8_t finalizedIndex) {
    const NbtCompound* villager = data.getCompound("VillagerData");
    if (villager == nullptr || villager->size() == 0) {
        return;
    }
    into.push_back(
        Entry{.index = dataIndex,
              .value = SetEntityDataPacket::VillagerDataValue{
                  .typeId = idOf(villagerTypeIds(), villager->getString("type")).value_or(2),
                  .professionId = idOf(villagerProfessionIds(), villager->getString("profession")).value_or(0),
                  .level = villager->getInt("level", 1)}});
    // Loading explicit VillagerData also flips DATA_VILLAGER_DATA_FINALIZED in both entity classes.
    into.push_back(Entry{.index = finalizedIndex, .value = SetEntityDataPacket::BoolValue{.value = true}});
}

void addIntIfSet(Entries& into, const NbtCompound& data, std::string_view key, std::uint8_t index) {
    const std::int32_t value = data.getInt(key);
    if (value != 0) {
        into.push_back(Entry{.index = index, .value = SetEntityDataPacket::IntValue{.value = value}});
    }
}

/// parity: `AbstractHorse`, whose tame flag every horse-like mob shares.
const std::set<std::string, std::less<>>& abstractHorseTypes() {
    static const std::set<std::string, std::less<>> kTypes{
        "horse", "donkey", "mule", "skeleton_horse", "zombie_horse", "llama", "trader_llama", "camel", "camel_husk"};
    return kTypes;
}

} // namespace

std::vector<Entry> generatedEntityMetadata(std::string_view type, const NbtCompound& data) {
    const wire::common::Identifier typeId = wire::common::Identifier::vanilla(std::string{type});
    Entries entries;
    std::int8_t sharedFlags = 0;
    if (data.getBoolean("Invisible")) {
        sharedFlags |= 0x20;
    }
    if (sharedFlags != 0) {
        entries.push_back(Entry{.index = 0, .value = SetEntityDataPacket::ByteValue{.value = sharedFlags}});
    }
    if (data.getBoolean("NoGravity")) {
        entries.push_back(Entry{.index = 5, .value = SetEntityDataPacket::BoolValue{.value = true}});
    }
    if (data.contains("Air") && data.getShort("Air") != kDefaultAirSupply) {
        entries.push_back(
            Entry{.index = kAirSupplyDataIndex, .value = SetEntityDataPacket::IntValue{.value = data.getShort("Air")}});
    }
    // Read as a number so older saves still count; zero makes the client play the death animation.
    // parity: `LivingEntity.DATA_HEALTH_ID`.
    if (data.contains("Health") && server::registry::entityAttributes::isLiving(typeId)) {
        entries.push_back(Entry{.index = 9,
                                .value = SetEntityDataPacket::FloatValue{
                                    .value = static_cast<float>(entityNbt::numericDouble(data, "Health"))}});
    }
    if (mobs::isMob(typeId)) {
        std::int8_t mobFlags = 0;
        if (data.getBoolean("NoAI")) {
            mobFlags |= 1;
        }
        if (data.getBoolean("LeftHanded")) {
            mobFlags |= 2;
        }
        if (mobFlags != 0) {
            entries.push_back(
                Entry{.index = kMobFlagsDataIndex, .value = SetEntityDataPacket::ByteValue{.value = mobFlags}});
        }
    }
    if (ageableMobs::hasBabyMetadata(type) && (entityNbt::numericInt(data, "Age") < 0 || data.getBoolean("IsBaby"))) {
        // parity: the piglin's baby flag sits after `AbstractPiglin`'s zombification one.
        entries.push_back(Entry{.index = static_cast<std::uint8_t>(type == "piglin" ? piglins::kBabyDataIndex
                                                                                    : ageableMobs::kBabyDataIndex),
                                .value = SetEntityDataPacket::BoolValue{.value = true}});
    }
    if (cubeMobs::isCube(type) && data.getInt("Size") != 0) {
        entries.push_back(Entry{.index = kCubeSizeDataIndex,
                                .value = SetEntityDataPacket::IntValue{.value = data.getInt("Size") + 1}});
    }

    if (type == "item_frame" || type == "glow_item_frame") {
        addItemFrameMetadata(entries, data);
    }
    // parity: `Cushion.DATA_COLOR`, sent only when it is not the default white.
    if (type == "cushion") {
        const std::optional<std::int32_t> colour = world::cushions::dyeIdOf(data.getString("color"));
        if (colour.has_value() && *colour != 0) {
            entries.push_back(Entry{
                .index = world::cushions::kColorDataIndex,
                .value = SetEntityDataPacket::RegistryIdValue{.value = *colour, .registry = RegistryHolder::DyeColor}});
        }
    }
    // parity: `Painting.DATA_PAINTING_VARIANT_ID`.
    if (type == "painting") {
        const std::string variant{data.getString("variant")};
        const std::string_view wanted = variant.starts_with("minecraft:")
                                            ? std::string_view{variant}.substr(std::string_view{"minecraft:"}.size())
                                            : std::string_view{variant};
        const std::vector<runtime::registry::vanillaRegistryData::Painting>& all =
            runtime::registry::vanillaRegistryData::paintingVariants();
        for (std::size_t index = 0; index < all.size(); ++index) {
            if (all[index].id != wanted) {
                continue;
            }
            entries.push_back(
                Entry{.index = kPaintingVariantIndex,
                      .value = SetEntityDataPacket::RegistryIdValue{.value = static_cast<std::int32_t>(index),
                                                                    .registry = RegistryHolder::PaintingVariant}});
            break;
        }
    }
    if (type == "mannequin") {
        addMannequinMetadata(entries, data);
    }
    if (type == "armor_stand") {
        addArmorStandMetadata(entries, data);
    } else if ((type == "zombie" || type == "husk") && data.contains("DrownedConversionTime") &&
               data.getInt("DrownedConversionTime") >= 0) {
        entries.push_back(Entry{.index = 18, .value = SetEntityDataPacket::BoolValue{.value = true}});
    } else if (type == "skeleton" && data.contains("StrayConversionTime") && data.getInt("StrayConversionTime") >= 0) {
        entries.push_back(Entry{.index = 16, .value = SetEntityDataPacket::BoolValue{.value = true}});
    } else if ((type == "piglin" || type == "piglin_brute" || type == "hoglin") &&
               data.getBoolean("IsImmuneToZombification")) {
        // parity: `AbstractPiglin`'s flag comes right after `Mob`'s.
        entries.push_back(Entry{.index = static_cast<std::uint8_t>(type == "hoglin" ? 18 : 16),
                                .value = SetEntityDataPacket::BoolValue{.value = true}});
    } else if (type == "creeper" && data.getBoolean("powered")) {
        entries.push_back(Entry{.index = 17, .value = SetEntityDataPacket::BoolValue{.value = true}});
    } else if (type == "mooshroom" && data.getString("Type") == "brown") {
        entries.push_back(Entry{.index = 18, .value = SetEntityDataPacket::IntValue{.value = 1}});
    } else if (type == "dolphin") {
        if (data.getBoolean("GotFish")) {
            entries.push_back(Entry{.index = 18, .value = SetEntityDataPacket::BoolValue{.value = true}});
        }
        if (data.contains("Moistness") && data.getInt("Moistness") != 2400) {
            entries.push_back(
                Entry{.index = 19, .value = SetEntityDataPacket::IntValue{.value = data.getInt("Moistness")}});
        }
    } else if (type == "wither") {
        addIntIfSet(entries, data, "Invul", 19);
    } else if (type == "wither_skull" && data.getBoolean("dangerous")) {
        entries.push_back(Entry{.index = 8, .value = SetEntityDataPacket::BoolValue{.value = true}});
    } else if (type == "axolotl") {
        addIntIfSet(entries, data, "Variant", axolotls::kVariantDataIndex);
    } else if (type == "cat") {
        addCatMetadata(entries, data);
    } else if (type == "chicken") {
        addRegistryMetadata(entries, data, "variant", 18, runtime::registry::vanillaRegistryData::chickenVariants(),
                            "minecraft:temperate", RegistryHolder::ChickenVariant);
        addRegistryMetadata(entries, data, "sound_variant", 19,
                            runtime::registry::vanillaRegistryData::chickenSoundVariants(), "minecraft:classic",
                            RegistryHolder::ChickenSoundVariant);
    } else if (type == "cow") {
        addRegistryMetadata(entries, data, "variant", 18, runtime::registry::vanillaRegistryData::cowVariants(),
                            "minecraft:temperate", RegistryHolder::CowVariant);
        addRegistryMetadata(entries, data, "sound_variant", 19,
                            runtime::registry::vanillaRegistryData::cowSoundVariants(), "minecraft:classic",
                            RegistryHolder::CowSoundVariant);
    } else if (type == "fox") {
        if (data.getString("Type") == "snow") {
            entries.push_back(Entry{.index = 19, .value = SetEntityDataPacket::IntValue{.value = 1}});
        }
    } else if (type == "frog") {
        addRegistryMetadata(entries, data, "variant", 18, runtime::registry::vanillaRegistryData::frogVariants(),
                            "minecraft:temperate", RegistryHolder::FrogVariant);
    } else if (type == "goat") {
        if (data.getBoolean("IsScreamingGoat")) {
            entries.push_back(Entry{.index = 18, .value = SetEntityDataPacket::BoolValue{.value = true}});
        }
        if (!data.getBoolean("HasLeftHorn", true)) {
            entries.push_back(Entry{.index = 19, .value = SetEntityDataPacket::BoolValue{.value = false}});
        }
        if (!data.getBoolean("HasRightHorn", true)) {
            entries.push_back(Entry{.index = 20, .value = SetEntityDataPacket::BoolValue{.value = false}});
        }
    } else if (type == "horse") {
        addIntIfSet(entries, data, "Variant", 19);
    } else if (type == "llama" || type == "trader_llama") {
        addIntIfSet(entries, data, "Strength", 20);
        addIntIfSet(entries, data, "Variant", 21);
    } else if (type == "parrot") {
        addIntIfSet(entries, data, "Variant", 20);
    } else if (type == "panda") {
        // parity: `Panda` fields start at 18.
        const std::optional<std::int32_t> main = idOf(pandaGeneIds(), data.getString("MainGene"));
        if (main.has_value() && *main != 0) {
            entries.push_back(
                Entry{.index = 21, .value = SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(*main)}});
        }
        const std::optional<std::int32_t> hidden = idOf(pandaGeneIds(), data.getString("HiddenGene"));
        if (hidden.has_value() && *hidden != 0) {
            entries.push_back(Entry{
                .index = 22, .value = SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(*hidden)}});
        }
    } else if (type == "pig") {
        addRegistryMetadata(entries, data, "variant", 19, runtime::registry::vanillaRegistryData::pigVariants(),
                            "minecraft:temperate", RegistryHolder::PigVariant);
        addRegistryMetadata(entries, data, "sound_variant", 20,
                            runtime::registry::vanillaRegistryData::pigSoundVariants(), "minecraft:classic",
                            RegistryHolder::PigSoundVariant);
    } else if (type == "rabbit") {
        addIntIfSet(entries, data, "RabbitType", 18);
    } else if (type == "pufferfish") {
        // parity: `Pufferfish.PUFF_STATE`, counted the same as the salmon's variant beside it.
        addIntIfSet(entries, data, "PuffState", kPufferfishPuffDataIndex);
    } else if (type == "salmon") {
        const std::int32_t variant = idOf(salmonVariants(), data.getString("type")).value_or(kSalmonMedium);
        if (variant != kSalmonMedium) {
            entries.push_back(Entry{.index = 17, .value = SetEntityDataPacket::IntValue{.value = variant}});
        }
    } else if (type == "sheep") {
        const std::int8_t wool =
            static_cast<std::int8_t>((data.getByte("Color") & 0x0F) | (data.getBoolean("Sheared") ? 0x10 : 0));
        if (wool != 0) {
            entries.push_back(Entry{.index = 18, .value = SetEntityDataPacket::ByteValue{.value = wool}});
        }
    } else if (type == "snow_golem") {
        // parity: `SnowGolem.DATA_PUMPKIN_ID`, which a fresh one starts with the pumpkin on.
        entries.push_back(Entry{.index = 16,
                                .value = SetEntityDataPacket::ByteValue{
                                    .value = static_cast<std::int8_t>(data.getBoolean("Pumpkin", true) ? 16 : 0)}});
    } else if (type == "bogged") {
        if (data.getBoolean("sheared")) {
            entries.push_back(Entry{.index = 16, .value = SetEntityDataPacket::BoolValue{.value = true}});
        }
    } else if (type == "turtle") {
        // parity: `Turtle.HAS_EGG`, the first index after `Animal`'s.
        if (data.getBoolean("HasEgg")) {
            entries.push_back(Entry{.index = 18, .value = SetEntityDataPacket::BoolValue{.value = true}});
        }
    } else if (type == "tropical_fish") {
        addIntIfSet(entries, data, "Variant", 17);
    } else if (type == "villager") {
        addVillagerMetadata(entries, data, 19, 20);
    } else if (type == "zombie_villager") {
        if (data.getBoolean("IsBaby")) {
            entries.push_back(Entry{.index = 16, .value = SetEntityDataPacket::BoolValue{.value = true}});
        }
        addVillagerMetadata(entries, data, 20, 21);
    } else if (type == "wolf") {
        addRegistryMetadata(entries, data, "variant", 23, runtime::registry::vanillaRegistryData::wolfVariants(),
                            "minecraft:pale", RegistryHolder::WolfVariant);
        addRegistryMetadata(entries, data, "sound_variant", 24,
                            runtime::registry::vanillaRegistryData::wolfSoundVariants(), "minecraft:classic",
                            RegistryHolder::WolfSoundVariant);
    } else if (type == "zombie_nautilus") {
        addRegistryMetadata(entries, data, "variant", 21,
                            runtime::registry::vanillaRegistryData::zombieNautilusVariants(), "minecraft:temperate",
                            RegistryHolder::ZombieNautilusVariant);
    }
    return entries;
}

std::vector<GeneratedEquipment> generatedEntityEquipment(const NbtCompound& data) {
    static constexpr std::array<std::pair<std::string_view, SetEquipmentPacket::SlotType>, 8> kSlots{
        {{"mainhand", SetEquipmentPacket::SlotType::MainHand},
         {"offhand", SetEquipmentPacket::SlotType::OffHand},
         {"feet", SetEquipmentPacket::SlotType::Feet},
         {"legs", SetEquipmentPacket::SlotType::Legs},
         {"chest", SetEquipmentPacket::SlotType::Chest},
         {"head", SetEquipmentPacket::SlotType::Head},
         {"body", SetEquipmentPacket::SlotType::Body},
         {"saddle", SetEquipmentPacket::SlotType::Saddle}}};
    std::vector<GeneratedEquipment> worn;
    const NbtCompound* equipment = data.getCompound("equipment");
    if (equipment == nullptr) {
        return worn;
    }
    for (const auto& [name, slot] : kSlots) {
        const NbtCompound* stack = equipment->getCompound(name);
        if (stack == nullptr) {
            continue;
        }
        const std::string_view item = stack->getString("id");
        const std::int32_t count = stack->getInt("count");
        if (item.empty() || count <= 0) {
            continue;
        }
        const NbtCompound* components = stack->getCompound("components");
        worn.push_back(GeneratedEquipment{.slot = slot,
                                          .item = std::string{item},
                                          .count = count,
                                          .components = components == nullptr ? NbtCompound{} : *components});
    }
    return worn;
}

std::vector<Entry> RuntimeEntityMetadata::changesFrom(const RuntimeEntityMetadata* previous) const {
    Entries entries;
    if (vehicle.has_value()) {
        appendVehicleChanges(entries, *vehicle,
                             previous == nullptr || !previous->vehicle.has_value() ? nullptr : &*previous->vehicle);
    }
    // parity: `FireworkRocketEntity`'s three fields, eight to ten.
    if (fireworkItem.has_value() && (previous == nullptr || previous->fireworkItem != fireworkItem)) {
        entries.push_back(Entry{.index = 8, .value = SetEntityDataPacket::ItemValue{.stack = *fireworkItem}});
    }
    if (fireworkAttached.has_value() && (previous == nullptr || previous->fireworkAttached != fireworkAttached) &&
        (previous != nullptr || fireworkAttached->has_value())) {
        entries.push_back(Entry{.index = 9, .value = SetEntityDataPacket::OptionalUnsignedIntValue{*fireworkAttached}});
    }
    if (arrows.has_value() && (previous == nullptr || previous->arrows != arrows) &&
        (previous != nullptr || *arrows != 0)) {
        entries.push_back(
            Entry{.index = kArrowCountDataIndex, .value = SetEntityDataPacket::IntValue{.value = *arrows}});
    }
    if (stingers.has_value() && (previous == nullptr || previous->stingers != stingers) &&
        (previous != nullptr || *stingers != 0)) {
        entries.push_back(
            Entry{.index = kStingerCountDataIndex, .value = SetEntityDataPacket::IntValue{.value = *stingers}});
    }
    if (armadilloState.has_value() && (previous == nullptr || previous->armadilloState != armadilloState) &&
        (previous != nullptr || *armadilloState != 0)) {
        entries.push_back(
            Entry{.index = armadillos::kStateDataIndex,
                  .value = SetEntityDataPacket::RegistryIdValue{
                      .value = *armadilloState, .registry = wire::protocol::play::RegistryHolder::ArmadilloState}});
    }
    if (snifferState.has_value() && (previous == nullptr || previous->snifferState != snifferState) &&
        (previous != nullptr || *snifferState != 0)) {
        entries.push_back(
            Entry{.index = sniffers::kStateDataIndex,
                  .value = SetEntityDataPacket::RegistryIdValue{
                      .value = *snifferState, .registry = wire::protocol::play::RegistryHolder::SnifferState}});
    }
    if (snifferDropSeed.has_value() && (previous == nullptr || previous->snifferDropSeed != snifferDropSeed) &&
        (previous != nullptr || *snifferDropSeed != 0)) {
        entries.push_back(Entry{.index = sniffers::kDropSeedDataIndex,
                                .value = SetEntityDataPacket::IntValue{.value = *snifferDropSeed}});
    }
    // parity: `Pufferfish.PUFF_STATE`, defined as zero, so only a puffed one is news.
    if (pufferfishPuff.has_value() && (previous == nullptr || previous->pufferfishPuff != pufferfishPuff) &&
        (previous != nullptr || *pufferfishPuff != 0)) {
        entries.push_back(
            Entry{.index = kPufferfishPuffDataIndex, .value = SetEntityDataPacket::IntValue{.value = *pufferfishPuff}});
    }
    if (camelPoseTick.has_value() && (previous == nullptr || previous->camelPoseTick != camelPoseTick) &&
        (previous != nullptr || *camelPoseTick != 0)) {
        entries.push_back(Entry{.index = camels::kLastPoseTickDataIndex,
                                .value = SetEntityDataPacket::LongValue{.value = *camelPoseTick}});
    }
    if (frogTongue.has_value() && (previous == nullptr || previous->frogTongue != frogTongue) &&
        (previous != nullptr || frogTongue->has_value())) {
        entries.push_back(Entry{.index = frogs::kTongueTargetDataIndex,
                                .value = SetEntityDataPacket::OptionalUnsignedIntValue{*frogTongue}});
    }
    if (fireworkShotAtAngle.has_value() &&
        (previous == nullptr || previous->fireworkShotAtAngle != fireworkShotAtAngle) &&
        (previous != nullptr || *fireworkShotAtAngle)) {
        entries.push_back(Entry{.index = 10, .value = SetEntityDataPacket::BoolValue{.value = *fireworkShotAtAngle}});
    }
    const auto flag = [&entries, previous](const std::optional<bool>& now,
                                           const std::optional<bool> RuntimeEntityMetadata::* field,
                                           std::uint8_t index) {
        if (now.has_value() && (previous == nullptr || previous->*field != now) && (previous != nullptr || *now)) {
            entries.push_back(Entry{.index = index, .value = SetEntityDataPacket::BoolValue{.value = *now}});
        }
    };
    flag(piglinBaby, &RuntimeEntityMetadata::piglinBaby, piglins::kBabyDataIndex);
    flag(piglinCharging, &RuntimeEntityMetadata::piglinCharging, piglins::kChargingDataIndex);
    flag(piglinDancing, &RuntimeEntityMetadata::piglinDancing, piglins::kDancingDataIndex);
    flag(axolotlPlayingDead, &RuntimeEntityMetadata::axolotlPlayingDead, axolotls::kPlayingDeadDataIndex);
    // parity: the horns default to there, so the first packet only says which is missing.
    const auto horn = [&entries, previous](const std::optional<bool>& now,
                                           const std::optional<bool> RuntimeEntityMetadata::* field,
                                           std::uint8_t index) {
        if (now.has_value() && (previous == nullptr || previous->*field != now) && (previous != nullptr || !*now)) {
            entries.push_back(Entry{.index = index, .value = SetEntityDataPacket::BoolValue{.value = *now}});
        }
    };
    horn(goatLeftHorn, &RuntimeEntityMetadata::goatLeftHorn, goats::kLeftHornDataIndex);
    horn(goatRightHorn, &RuntimeEntityMetadata::goatRightHorn, goats::kRightHornDataIndex);
    if (mobFlags.has_value() && (previous == nullptr || previous->mobFlags != mobFlags) &&
        (previous != nullptr || *mobFlags != 0)) {
        entries.push_back(
            Entry{.index = kMobFlagsDataIndex, .value = SetEntityDataPacket::ByteValue{.value = *mobFlags}});
    }
    if (foxFlags.has_value() && (previous == nullptr || previous->foxFlags != foxFlags) &&
        (previous != nullptr || *foxFlags != 0)) {
        entries.push_back(
            Entry{.index = kFoxFlagsDataIndex, .value = SetEntityDataPacket::ByteValue{.value = *foxFlags}});
    }
    if (pandaFlags.has_value() && (previous == nullptr || previous->pandaFlags != pandaFlags) &&
        (previous != nullptr || *pandaFlags != 0)) {
        entries.push_back(
            Entry{.index = kPandaFlagsDataIndex, .value = SetEntityDataPacket::ByteValue{.value = *pandaFlags}});
    }
    const auto counter = [&](const std::optional<std::int32_t>& now, const std::optional<std::int32_t>& before,
                             std::uint8_t index) {
        if (now.has_value() && (previous == nullptr || before != now) && (previous != nullptr || *now != 0)) {
            entries.push_back(Entry{.index = index, .value = SetEntityDataPacket::IntValue{.value = *now}});
        }
    };
    counter(pandaUnhappy, previous == nullptr ? std::nullopt : previous->pandaUnhappy, kPandaUnhappyDataIndex);
    counter(pandaSneeze, previous == nullptr ? std::nullopt : previous->pandaSneeze, kPandaSneezeDataIndex);
    counter(pandaEat, previous == nullptr ? std::nullopt : previous->pandaEat, kPandaEatDataIndex);
    const auto plain = [&](const std::optional<bool>& now, const std::optional<bool> RuntimeEntityMetadata::* field,
                           std::uint8_t index) {
        if (now.has_value() && (previous == nullptr || previous->*field != now) && (previous != nullptr || *now)) {
            entries.push_back(Entry{.index = index, .value = SetEntityDataPacket::BoolValue{.value = *now}});
        }
    };
    // parity: the first index each of these mobs defines after `Animal`'s, or `Raider`'s for the witch.
    plain(polarStanding, &RuntimeEntityMetadata::polarStanding, 18);
    plain(striderCold, &RuntimeEntityMetadata::striderCold, 19);
    plain(turtleLaying, &RuntimeEntityMetadata::turtleLaying, 19);
    plain(witchUsing, &RuntimeEntityMetadata::witchUsing, 17);
    if (boostTotal.has_value() && (previous == nullptr || previous->boostTotal != boostTotal) &&
        (previous != nullptr || *boostTotal != 0)) {
        entries.push_back(Entry{.index = 18, .value = SetEntityDataPacket::IntValue{.value = *boostTotal}});
    }
    if (allayDancing.has_value() && (previous == nullptr || previous->allayDancing != allayDancing) &&
        (previous != nullptr || *allayDancing)) {
        entries.push_back(
            Entry{.index = allays::kDancingDataIndex, .value = SetEntityDataPacket::BoolValue{.value = *allayDancing}});
    }
    if (allayCanDuplicate.has_value() && (previous == nullptr || previous->allayCanDuplicate != allayCanDuplicate) &&
        (previous != nullptr || !*allayCanDuplicate)) {
        entries.push_back(Entry{.index = allays::kCanDuplicateDataIndex,
                                .value = SetEntityDataPacket::BoolValue{.value = *allayCanDuplicate}});
    }
    if (beeFlags.has_value() && (previous == nullptr || previous->beeFlags != beeFlags) &&
        (previous != nullptr || *beeFlags != 0)) {
        entries.push_back(Entry{.index = bees::kFlagsDataIndex, .value = SetEntityDataPacket::ByteValue{*beeFlags}});
    }
    if (beeAngerEnd.has_value() && (previous == nullptr || previous->beeAngerEnd != beeAngerEnd) &&
        (previous != nullptr || *beeAngerEnd != -1)) {
        entries.push_back(
            Entry{.index = bees::kAngerEndDataIndex, .value = SetEntityDataPacket::LongValue{*beeAngerEnd}});
    }
    if (wolfAngerEnd.has_value() && (previous == nullptr || previous->wolfAngerEnd != wolfAngerEnd) &&
        (previous != nullptr || *wolfAngerEnd != -1)) {
        entries.push_back(
            Entry{.index = kWolfAngerEndDataIndex, .value = SetEntityDataPacket::LongValue{*wolfAngerEnd}});
    }
    if (wolfInterested.has_value() && (previous == nullptr || previous->wolfInterested != wolfInterested) &&
        (previous != nullptr || *wolfInterested)) {
        entries.push_back(
            Entry{.index = kWolfInterestedDataIndex, .value = SetEntityDataPacket::BoolValue{*wolfInterested}});
    }
    if (endermanCreepy.has_value() && (previous == nullptr || previous->endermanCreepy != endermanCreepy) &&
        (previous != nullptr || *endermanCreepy)) {
        entries.push_back(
            Entry{.index = kEndermanCreepyDataIndex, .value = SetEntityDataPacket::BoolValue{*endermanCreepy}});
    }
    if (endermanStaredAt.has_value() && (previous == nullptr || previous->endermanStaredAt != endermanStaredAt) &&
        (previous != nullptr || *endermanStaredAt)) {
        entries.push_back(
            Entry{.index = kEndermanStaredAtDataIndex, .value = SetEntityDataPacket::BoolValue{*endermanStaredAt}});
    }
    if ((previous == nullptr || previous->airSupply != airSupply) &&
        (previous != nullptr || airSupply != kDefaultAirSupply)) {
        entries.push_back(
            Entry{.index = kAirSupplyDataIndex, .value = SetEntityDataPacket::IntValue{.value = airSupply}});
    }
    if (baby.has_value() && (previous == nullptr || previous->baby != baby) && (previous != nullptr || *baby)) {
        entries.push_back(
            Entry{.index = ageableMobs::kBabyDataIndex, .value = SetEntityDataPacket::BoolValue{.value = *baby}});
    }
    if (cubeSize.has_value() && (previous == nullptr || previous->cubeSize != cubeSize) &&
        (previous != nullptr || *cubeSize != cubeMobs::kDefaultSize)) {
        entries.push_back(
            Entry{.index = kCubeSizeDataIndex, .value = SetEntityDataPacket::IntValue{.value = *cubeSize}});
    }
    if (livingFlags.has_value() && (previous == nullptr || previous->livingFlags != livingFlags) &&
        (previous != nullptr || *livingFlags != 0)) {
        entries.push_back(Entry{.index = SetEntityDataPacket::kLivingFlagsIndex,
                                .value = SetEntityDataPacket::ByteValue{.value = *livingFlags}});
    }
    if (horseFlags.has_value() && (previous == nullptr || previous->horseFlags != horseFlags) &&
        (previous != nullptr || *horseFlags != 0)) {
        entries.push_back(Entry{.index = kAbstractHorseFlagsDataIndex,
                                .value = SetEntityDataPacket::ByteValue{.value = *horseFlags}});
    }
    for (const auto& [now, before] : {std::pair{horseChest, previous == nullptr ? std::nullopt : previous->horseChest},
                                      std::pair{camelDash, previous == nullptr ? std::nullopt : previous->camelDash}}) {
        if (now.has_value() && (previous == nullptr ? *now : before != now)) {
            entries.push_back(
                Entry{.index = horses::kChestDataIndex, .value = SetEntityDataPacket::BoolValue{.value = *now}});
        }
    }
    for (const auto& [index, now, before, unusual] :
         {std::tuple{creakings::kCanMoveDataIndex, creakingCanMove,
                     previous == nullptr ? std::nullopt : previous->creakingCanMove, false},
          std::tuple{creakings::kActiveDataIndex, creakingActive,
                     previous == nullptr ? std::nullopt : previous->creakingActive, true},
          std::tuple{creakings::kTearingDownDataIndex, creakingTearingDown,
                     previous == nullptr ? std::nullopt : previous->creakingTearingDown, true}}) {
        if (now.has_value() && (previous == nullptr ? *now == unusual : before != now)) {
            entries.push_back(Entry{.index = index, .value = SetEntityDataPacket::BoolValue{.value = *now}});
        }
    }
    if (tridentLoyalty.has_value() &&
        (previous == nullptr ? *tridentLoyalty != 0 : previous->tridentLoyalty != tridentLoyalty)) {
        entries.push_back(Entry{.index = kTridentLoyaltyDataIndex,
                                .value = SetEntityDataPacket::ByteValue{.value = *tridentLoyalty}});
    }
    if (tridentFoil.has_value() && (previous == nullptr ? *tridentFoil : previous->tridentFoil != tridentFoil)) {
        entries.push_back(
            Entry{.index = kTridentFoilDataIndex, .value = SetEntityDataPacket::BoolValue{.value = *tridentFoil}});
    }
    if (creakingHome.has_value() && (previous == nullptr || previous->creakingHome != creakingHome) &&
        (previous != nullptr || creakingHome->has_value())) {
        entries.push_back(Entry{.index = creakings::kHomeDataIndex,
                                .value = SetEntityDataPacket::OptionalBlockPosValue{.pos = *creakingHome}});
    }
    if (crystalBeam.has_value() && (previous == nullptr || previous->crystalBeam != crystalBeam) &&
        (previous != nullptr || crystalBeam->has_value())) {
        entries.push_back(Entry{.index = world::endCrystals::kBeamTargetDataIndex,
                                .value = SetEntityDataPacket::OptionalBlockPosValue{.pos = *crystalBeam}});
    }
    if (crystalBottom.has_value() && (previous == nullptr || previous->crystalBottom != crystalBottom) &&
        (previous != nullptr || !*crystalBottom)) {
        entries.push_back(Entry{.index = world::endCrystals::kShowBottomDataIndex,
                                .value = SetEntityDataPacket::BoolValue{.value = *crystalBottom}});
    }
    if (cloudRadius.has_value() && (previous == nullptr || previous->cloudRadius != cloudRadius) &&
        (previous != nullptr || *cloudRadius != areaEffectClouds::kDefaultRadius)) {
        entries.push_back(Entry{.index = areaEffectClouds::kRadiusDataIndex,
                                .value = SetEntityDataPacket::FloatValue{.value = *cloudRadius}});
    }
    if (cloudWaiting.has_value() && (previous == nullptr || previous->cloudWaiting != cloudWaiting) &&
        (previous != nullptr || *cloudWaiting)) {
        entries.push_back(Entry{.index = areaEffectClouds::kWaitingDataIndex,
                                .value = SetEntityDataPacket::BoolValue{.value = *cloudWaiting}});
    }
    if (cloudParticle.has_value() && (previous == nullptr || previous->cloudParticle != cloudParticle)) {
        entries.push_back(Entry{.index = areaEffectClouds::kParticleDataIndex, .value = *cloudParticle});
    }
    if (orbValue.has_value() && (previous == nullptr || previous->orbValue != orbValue) &&
        (previous != nullptr || *orbValue != 0)) {
        entries.push_back(
            Entry{.index = kOrbValueDataIndex, .value = SetEntityDataPacket::IntValue{.value = *orbValue}});
    }
    if (hookedEntity.has_value() && (previous == nullptr || previous->hookedEntity != hookedEntity) &&
        (previous != nullptr || *hookedEntity != 0)) {
        entries.push_back(Entry{.index = fishingHooks::kHookedDataIndex,
                                .value = SetEntityDataPacket::IntValue{.value = *hookedEntity}});
    }
    if (hookBiting.has_value() && (previous == nullptr || previous->hookBiting != hookBiting) &&
        (previous != nullptr || *hookBiting)) {
        entries.push_back(Entry{.index = fishingHooks::kBitingDataIndex,
                                .value = SetEntityDataPacket::BoolValue{.value = *hookBiting}});
    }
    if (effectParticles.has_value() && (previous == nullptr || previous->effectParticles != effectParticles) &&
        (previous != nullptr || !effectParticles->particles.empty())) {
        entries.push_back(Entry{.index = kEffectParticlesDataIndex, .value = *effectParticles});
    }
    if (effectsAmbient.has_value() && (previous == nullptr || previous->effectsAmbient != effectsAmbient) &&
        (previous != nullptr || *effectsAmbient)) {
        entries.push_back(Entry{.index = kEffectAmbienceDataIndex,
                                .value = SetEntityDataPacket::BoolValue{.value = *effectsAmbient}});
    }
    if (pose.has_value() && (previous == nullptr || previous->pose != pose) &&
        (previous != nullptr || *pose != wire::protocol::play::Pose::Standing)) {
        entries.push_back(
            Entry{.index = SetEntityDataPacket::kPoseIndex, .value = SetEntityDataPacket::PoseValue{.pose = *pose}});
    }
    if (wardenAnger.has_value() && (previous == nullptr || previous->wardenAnger != wardenAnger) &&
        (previous != nullptr || *wardenAnger != 0)) {
        entries.push_back(Entry{.index = wardenBrain::kAngerDataIndex,
                                .value = SetEntityDataPacket::IntValue{.value = *wardenAnger}});
    }
    // parity: `DATA_DANGEROUS`, the first index a `WitherSkull` adds after `Entity`'s own.
    constexpr std::uint8_t kDangerousSkullIndex = 8;
    if (dangerousSkull.has_value() && (previous == nullptr || previous->dangerousSkull != dangerousSkull) &&
        (previous != nullptr || *dangerousSkull)) {
        entries.push_back(
            Entry{.index = kDangerousSkullIndex, .value = SetEntityDataPacket::BoolValue{.value = *dangerousSkull}});
    }
    const auto copperEntry = [&](const std::optional<std::int32_t> RuntimeEntityMetadata::* field, std::uint8_t index,
                                 wire::protocol::play::RegistryHolder holder) {
        const std::optional<std::int32_t>& value = this->*field;
        if (value.has_value() && (previous == nullptr || previous->*field != value) &&
            (previous != nullptr || *value != 0)) {
            entries.push_back(Entry{
                .index = index, .value = SetEntityDataPacket::RegistryIdValue{.value = *value, .registry = holder}});
        }
    };
    copperEntry(&RuntimeEntityMetadata::copperWeather, copperGolems::kWeatherDataIndex,
                wire::protocol::play::RegistryHolder::WeatheringCopperState);
    copperEntry(&RuntimeEntityMetadata::copperPose, copperGolems::kStateDataIndex,
                wire::protocol::play::RegistryHolder::CopperGolemState);
    // parity: `Shulker.defineSynchedData`: down, closed and without a colour.
    if (shulkerFace.has_value() && (previous == nullptr || previous->shulkerFace != shulkerFace) &&
        (previous != nullptr || *shulkerFace != 0)) {
        entries.push_back(
            Entry{.index = shulkers::kAttachFaceDataIndex,
                  .value = SetEntityDataPacket::RegistryIdValue{
                      .value = *shulkerFace, .registry = wire::protocol::play::RegistryHolder::Direction}});
    }
    if (shulkerPeek.has_value() && (previous == nullptr || previous->shulkerPeek != shulkerPeek) &&
        (previous != nullptr || *shulkerPeek != 0)) {
        entries.push_back(
            Entry{.index = shulkers::kPeekDataIndex,
                  .value = SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(*shulkerPeek)}});
    }
    if (shulkerColor.has_value() && (previous == nullptr || previous->shulkerColor != shulkerColor) &&
        (previous != nullptr || *shulkerColor != shulkers::kNoColor)) {
        entries.push_back(
            Entry{.index = shulkers::kColorDataIndex,
                  .value = SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(*shulkerColor)}});
    }
    constexpr std::uint8_t kNameIndex = 2;
    constexpr std::uint8_t kNameVisibleIndex = 3;
    constexpr std::uint8_t kSilentIndex = 4;
    constexpr std::uint8_t kNoGravityIndex = 5;
    if ((previous == nullptr && sharedFlags != 0) || (previous != nullptr && previous->sharedFlags != sharedFlags)) {
        entries.push_back(Entry{.index = SetEntityDataPacket::kSharedFlagsIndex,
                                .value = SetEntityDataPacket::ByteValue{.value = sharedFlags}});
    }
    if ((previous == nullptr && customName.has_value()) ||
        (previous != nullptr && previous->customName != customName)) {
        entries.push_back(
            Entry{.index = kNameIndex,
                  .value = SetEntityDataPacket::OptionalTextValue{
                      .value = customName.has_value() ? std::optional{wire::text::Component::text(*customName)}
                                                      : std::nullopt}});
    }
    const auto flagEntry = [&](bool now, bool RuntimeEntityMetadata::* field, std::uint8_t index) {
        if ((previous == nullptr && now) || (previous != nullptr && previous->*field != now)) {
            entries.push_back(Entry{.index = index, .value = SetEntityDataPacket::BoolValue{.value = now}});
        }
    };
    flagEntry(nameVisible, &RuntimeEntityMetadata::nameVisible, kNameVisibleIndex);
    flagEntry(silent, &RuntimeEntityMetadata::silent, kSilentIndex);
    flagEntry(noGravity, &RuntimeEntityMetadata::noGravity, kNoGravityIndex);
    // parity: a fresh dragon is defined hovering, so only a different phase is news.
    if (dragonPhase.has_value() && (previous == nullptr || previous->dragonPhase != dragonPhase) &&
        (previous != nullptr || *dragonPhase != static_cast<std::int32_t>(enderDragon::Phase::Hover))) {
        entries.push_back(Entry{.index = enderDragon::kPhaseDataIndex,
                                .value = SetEntityDataPacket::IntValue{.value = *dragonPhase}});
    }
    return entries;
}

std::int8_t sharedFlagsOf(const ServerEntity& entity) {
    const bool hasData = entity.sourceNbt.has_value();
    const bool invisible = (hasData && entity.sourceNbt->getBoolean("Invisible")) ||
                           entity.effects.contains(wire::common::Identifier::vanilla("invisibility"));
    const bool glowing = (hasData && entity.sourceNbt->getBoolean("Glowing")) ||
                         entity.effects.contains(wire::common::Identifier::vanilla("glowing"));
    std::int8_t flags = 0;
    if (entity.visualFire < 0 ? entity.fireTicks > 0 : entity.visualFire > 0) {
        flags |= SetEntityDataPacket::kFlagOnFire;
    }
    flags |= static_cast<std::int8_t>(entity.forcedFlags);
    if (invisible) {
        flags |= 0x20;
    }
    if (glowing) {
        flags |= 0x40;
    }
    return flags;
}

RuntimeEntityMetadata runtimeEntityMetadata(const ServerEntity& entity) {
    RuntimeEntityMetadata live{.airSupply = entity.airSupply};
    live.sharedFlags = sharedFlagsOf(entity);
    live.customName = entity.customName;
    live.nameVisible = entity.sourceNbt.has_value() && entity.sourceNbt->getBoolean("CustomNameVisible");
    live.silent = entity.sourceNbt.has_value() && entity.sourceNbt->getBoolean("Silent");
    live.noGravity = entity.noGravity;
    if (entity.type().path() == "fox") {
        constexpr std::int32_t kSitting = 1;
        constexpr std::int32_t kSleeping = 32;
        live.foxFlags = static_cast<std::int8_t>(entity.foxFlags | (entity.sitting ? kSitting : 0) |
                                                 (entity.sleeping ? kSleeping : 0));
    } else if (entity.type().path() == "panda") {
        live.pandaFlags = static_cast<std::int8_t>(entity.pandaFlags);
        live.pandaUnhappy = entity.pandaUnhappy;
        live.pandaSneeze = entity.pandaSneeze;
        live.pandaEat = entity.pandaEat;
    }
    if (mobs::isMob(entity.type())) {
        live.mobFlags = static_cast<std::int8_t>(
            (entity.noAi ? 1 : 0) |
            (entity.sourceNbt.has_value() && entity.sourceNbt->getBoolean("LeftHanded") ? 2 : 0) |
            (entity.aggressive ? 4 : 0));
    }
    live.vehicle = vehicleDataOf(entity);
    if (ageableMobs::hasBabyMetadata(entity.type().path())) {
        live.baby = entity.age < 0;
    }
    if (cubeMobs::isCube(entity.type())) {
        live.cubeSize = entity.cubeSize;
    }
    if (abstractHorseTypes().contains(entity.type().path())) {
        live.horseFlags = horses::flagsOf(entity);
        if (horses::isChested(entity.type())) {
            live.horseChest = horses::hasChest(entity);
        }
        if (horses::isCamel(entity.type())) {
            live.camelDash = entity.horse != nullptr && entity.horse->dashing;
        }
    }
    if (entity.type().path() == "experience_orb" && entity.sourceNbt.has_value()) {
        live.orbValue = entity.sourceNbt->getShort("Value");
    }
    if (entity.type().path() == "pufferfish") {
        live.pufferfishPuff = entity.puffState;
    }
    if (server::registry::entityAttributes::isLiving(entity.type())) {
        const mobStatusEffects::Swirl swirl = mobStatusEffects::swirlOf(entity.effects);
        SetEntityDataPacket::ParticlesValue particles;
        for (const mobStatusEffects::Swirl::Particle& particle : swirl.particles) {
            const server::registry::ParticleType* type = server::registry::particleTypes::of(particle.particle);
            if (type == nullptr) {
                continue;
            }
            particles.particles.push_back(SetEntityDataPacket::ParticleValue{
                .particleId = type->protocolId,
                .options = particle.argb.has_value()
                               ? std::optional{wire::protocol::play::ParticleOptions{
                                     wire::protocol::play::ParticleOptions::Color{*particle.argb}}}
                               : std::nullopt});
        }
        live.effectParticles = std::move(particles);
        live.stingers = entity.stingerCount;
        live.arrows = entity.arrowCount;
        // parity: `areAllEffectsAmbient` over no effects at all answers false on a fresh entity.
        live.effectsAmbient = !entity.effects.empty() && swirl.allAmbient;
    }
    if (wardens::isWarden(entity.type())) {
        live.pose = wardenBrain::poseOf(entity);
        // parity: `AngerManagement.getActiveAnger`: the target's, or the only suspect's.
        if (entity.targetId.has_value()) {
            live.wardenAnger = wardens::angerAt(entity, *entity.targetId);
        } else {
            live.wardenAnger =
                entity.anger.has_value() && entity.anger->size() == 1 ? entity.anger->begin()->second : 0;
        }
    }
    if (entity.type().path() == "wither_skull") {
        live.dangerousSkull = entity.dangerousSkull;
    }
    if (entity.copperGolem != nullptr) {
        live.copperWeather = static_cast<std::int32_t>(entity.copperGolem->weather);
        live.copperPose = static_cast<std::int32_t>(entity.copperGolem->pose);
    }
    if (entity.shulker != nullptr) {
        live.shulkerFace = static_cast<std::int32_t>(entity.shulker->attachFace);
        live.shulkerPeek = entity.shulker->peek;
        live.shulkerColor = entity.shulker->color;
    }
    if (entity.hook != nullptr) {
        live.hookedEntity = entity.hook->hookedIn.has_value() ? *entity.hook->hookedIn + 1 : 0;
        live.hookBiting = entity.hook->biting;
    }
    if (entity.dragon != nullptr) {
        live.dragonPhase = static_cast<std::int32_t>(entity.dragon->phase);
    }
    if (entity.goat != nullptr) {
        live.goatLeftHorn = !entity.sourceNbt.has_value() || entity.sourceNbt->getBoolean("HasLeftHorn", true);
        live.goatRightHorn = !entity.sourceNbt.has_value() || entity.sourceNbt->getBoolean("HasRightHorn", true);
        if (entity.goat->jump.midJump) {
            live.pose = wire::protocol::play::Pose::LongJumping;
        } else {
            live.pose = wire::protocol::play::Pose::Standing;
        }
    }
    if (entity.breeze != nullptr) {
        static constexpr std::array<wire::protocol::play::Pose, 5> kBreezePoses{
            wire::protocol::play::Pose::Standing, wire::protocol::play::Pose::Sliding,
            wire::protocol::play::Pose::LongJumping, wire::protocol::play::Pose::Inhaling,
            wire::protocol::play::Pose::Shooting};
        live.pose = kBreezePoses[static_cast<std::size_t>(entity.breeze->pose)];
    }
    if (entity.armadillo != nullptr) {
        live.armadilloState = static_cast<std::int32_t>(entity.armadillo->state);
    }
    if (entity.sniffer != nullptr) {
        live.snifferState = static_cast<std::int32_t>(entity.sniffer->pose);
        live.snifferDropSeed = entity.sniffer->dropSeedAt;
    }
    if (entity.camel != nullptr) {
        live.camelPoseTick = entity.camel->lastPoseTick;
        live.pose =
            camels::sitting(*entity.camel) ? wire::protocol::play::Pose::Sitting : wire::protocol::play::Pose::Standing;
    }
    if (entity.frog != nullptr) {
        live.frogTongue = entity.frog->tongueTarget;
        static constexpr std::array<wire::protocol::play::Pose, 4> kFrogPoses{
            wire::protocol::play::Pose::Standing, wire::protocol::play::Pose::Croaking,
            wire::protocol::play::Pose::UsingTongue, wire::protocol::play::Pose::LongJumping};
        live.pose = kFrogPoses[static_cast<std::size_t>(entity.frog->pose)];
    }
    if (entity.axolotl != nullptr) {
        live.axolotlPlayingDead = axolotls::playingDead(entity);
    }
    if (entity.type().path() == "piglin") {
        live.baby.reset();
        live.piglinBaby = entity.age < 0;
        if (entity.piglin != nullptr) {
            live.piglinCharging = entity.piglin->chargingCrossbow;
            live.piglinDancing = entity.piglin->dancing;
        }
    }
    if (const mobMisc::State* held = mobMisc::peek(entity); held != nullptr) {
        const std::string_view kind = entity.type().path();
        if (kind == "polar_bear") {
            live.polarStanding = held->polarStanding;
        } else if (kind == "strider") {
            live.striderCold = held->striderCold;
            live.boostTotal = held->boostTotal;
        } else if (kind == "pig") {
            live.boostTotal = held->boostTotal;
        } else if (kind == "turtle") {
            live.turtleLaying = held->turtleLaying;
        } else if (kind == "witch") {
            live.witchUsing = held->witchDrinking;
        }
    }
    if (entity.allay != nullptr) {
        live.allayDancing = entity.allay->dancing;
        live.allayCanDuplicate = entity.allay->duplicationCooldown == 0;
    }
    if (entity.bee != nullptr) {
        live.beeFlags = bees::flagsOf(*entity.bee);
        live.beeAngerEnd = entity.bee->angerEndTime;
    }
    if (entity.type().path() == "wolf") {
        live.wolfAngerEnd = entity.neutral != nullptr ? entity.neutral->endTime : -1;
        live.wolfInterested = entity.begging;
    }
    if (entity.type().path() == "enderman") {
        // parity: `setTarget` raises `DATA_CREEPY` with a target and lowers both without one.
        live.endermanCreepy = entity.targetId.has_value();
        live.endermanStaredAt = entity.neutral != nullptr && entity.neutral->staredAt;
    }
    if (entity.firework != nullptr) {
        const wire::nbt::NbtCompound& item = entity.firework->item;
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(std::string{item.getString("id")});
        const server::registry::Item* type = id.has_value() ? server::registry::items::registry().byName(*id) : nullptr;
        if (type != nullptr) {
            const wire::nbt::NbtCompound* components = item.getCompound("components");
            live.fireworkItem = wire::protocol::play::WireItemStack{
                .itemId = type->protocolId,
                .count = item.getInt("count", 1),
                .components = components == nullptr
                                  ? std::vector<wire::protocol::play::ItemComponent>{}
                                  : item::itemComponentCodecs::toWire(*components, [](const wire::common::Identifier&) {
                                        return std::optional<std::int32_t>{};
                                    })};
        }
        live.fireworkAttached = entity.firework->attachedTo;
        live.fireworkShotAtAngle = entity.firework->shotAtAngle;
    }
    if (areaEffectClouds::isCloud(entity.type())) {
        const areaEffectClouds::Cloud cloud = areaEffectClouds::read(entity);
        live.cloudRadius = cloud.radius;
        const std::int32_t waitTime = entity.sourceNbt.has_value() ? entity.sourceNbt->getInt("WaitTime", 20) : 20;
        live.cloudWaiting = entity.tickCount < waitTime;
        if (cloud.particle.has_value()) {
            const auto id = wire::common::Identifier::parse(*cloud.particle);
            const server::registry::ParticleType* type =
                id.has_value() ? server::registry::particleTypes::of(*id) : nullptr;
            if (type != nullptr) {
                // parity: only `dragon_breath` among the custom particles a cloud is given carries a power.
                live.cloudParticle = SetEntityDataPacket::ParticleValue{
                    .particleId = type->protocolId,
                    .options = id->path() == "dragon_breath"
                                   ? std::optional{wire::protocol::play::ParticleOptions{
                                         wire::protocol::play::ParticleOptions::Number{cloud.particlePower}}}
                                   : std::nullopt};
            }
        }
    }
    if (entity.type().path() == "trident") {
        live.tridentLoyalty = std::max<std::int8_t>(entity.tridentLoyalty, 0);
        live.tridentFoil = entity.tridentFoil;
    }
    if (creakings::isCreaking(entity.type())) {
        const creakings::CreakingState* state = entity.creaking.get();
        live.creakingCanMove = state == nullptr || state->canMove;
        live.creakingActive = state != nullptr && state->active;
        live.creakingTearingDown = state != nullptr && state->tearingDown;
        live.creakingHome = std::nullopt;
        if (state != nullptr && state->home.has_value()) {
            live.creakingHome =
                wire::protocol::play::BlockPos{.x = (*state->home)[0], .y = (*state->home)[1], .z = (*state->home)[2]};
        }
    }
    if (world::endCrystals::isCrystal(entity.type())) {
        const std::optional<std::array<std::int32_t, 3>> beam = world::endCrystals::beamTarget(entity);
        live.crystalBeam =
            beam.has_value()
                ? std::optional{wire::protocol::play::BlockPos{.x = (*beam)[0], .y = (*beam)[1], .z = (*beam)[2]}}
                : std::nullopt;
        live.crystalBottom = world::endCrystals::showsBottom(entity);
    }
    if (entity.activeSlot >= 0 || entity.riptiding) {
        live.livingFlags = static_cast<std::int8_t>((entity.activeSlot >= 0 ? 1 : 0) |
                                                    (entity.activeSlot == 1 ? 2 : 0) | (entity.riptiding ? 4 : 0));
    } else if (server::registry::entityAttributes::isLiving(entity.type())) {
        live.livingFlags = 0;
    }
    if (entity.forcedPose >= 0) {
        live.pose = static_cast<wire::protocol::play::Pose>(entity.forcedPose);
    } else if ((entity.forcedFlags & 0x80) != 0) {
        live.pose = wire::protocol::play::Pose::FallFlying;
    } else if ((entity.forcedFlags & 0x10) != 0) {
        live.pose = wire::protocol::play::Pose::Swimming;
    } else if ((entity.forcedFlags & 0x02) != 0) {
        live.pose = wire::protocol::play::Pose::Crouching;
    } else if (entity.forcedPose == -2 && !live.pose.has_value()) {
        live.pose = wire::protocol::play::Pose::Standing;
    }
    return live;
}

} // namespace basalt::server::runtime::entity
