#include "GeneratedEntityShared.hpp"

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

namespace basalt::server::runtime::entity {
using namespace presentation;

namespace {

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
            entries.push_back(Entry{.index = kFoxVariantDataIndex, .value = SetEntityDataPacket::IntValue{.value = 1}});
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

} // namespace basalt::server::runtime::entity
