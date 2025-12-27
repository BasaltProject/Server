#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <array>
#include <map>
#include <set>
#include <utility>

namespace basalt::server::runtime::entity {
namespace {

using wire::nbt::NbtCompound;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SetEquipmentPacket;
using Entry = SetEntityDataPacket::Entry;
using Entries = std::vector<Entry>;
using RegistryHolder = wire::protocol::play::RegistryHolder;

/// `AbstractCubeMob.ID_SIZE`: two `AgeableMob` fields follow the common index 16.
constexpr std::uint8_t kCubeSizeDataIndex = 18;
constexpr std::uint8_t kAbstractHorseFlagsDataIndex = 18;
constexpr std::int8_t kAbstractHorseTameFlag = 2;
constexpr std::uint8_t kMobFlagsDataIndex = 15;
constexpr std::uint8_t kAirSupplyDataIndex = 1;
constexpr std::int32_t kDefaultAirSupply = 300;

using Ids = std::map<std::string, std::int32_t, std::less<>>;

/// The ids of a synced registry are its entry order, which is what the client resolves against.
Ids registryIds(std::initializer_list<std::string_view> paths) {
    Ids ids;
    std::int32_t index = 0;
    for (const std::string_view path : paths) {
        ids.emplace("minecraft:" + std::string{path}, index++);
    }
    return ids;
}

const Ids& catVariantIds() {
    static const Ids kIds{{"minecraft:tabby", 0},
                          {"minecraft:black", 1},
                          {"minecraft:red", 2},
                          {"minecraft:siamese", 3},
                          {"minecraft:british_shorthair", 4},
                          {"minecraft:calico", 5},
                          {"minecraft:persian", 6},
                          {"minecraft:ragdoll", 7},
                          {"minecraft:white", 8},
                          {"minecraft:jellie", 9},
                          {"minecraft:all_black", 10}};
    return kIds;
}

/// Black is the `defineSynchedData` default, so vanilla's initial packet never carries it.
constexpr std::int32_t kCatVariantBlack = 1;

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

void addRegistryMetadata(Entries& into, const NbtCompound& data, std::string_view key, std::uint8_t index,
                         const Ids& ids, RegistryHolder registry) {
    const std::optional<std::int32_t> id = idOf(ids, data.getString(key));
    if (id.has_value() && *id != 0) {
        into.push_back(
            Entry{.index = index, .value = SetEntityDataPacket::RegistryIdValue{.value = *id, .registry = registry}});
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
    const std::optional<std::int32_t> variant = idOf(catVariantIds(), data.getString("variant"));
    if (variant.has_value() && *variant != kCatVariantBlack) {
        into.push_back(Entry{
            .index = 20,
            .value = SetEntityDataPacket::RegistryIdValue{.value = *variant, .registry = RegistryHolder::CatVariant}});
    }
    const std::int32_t collarColor = data.getByte("CollarColor", 14) & 0xFF;
    if (collarColor != 14) {
        into.push_back(Entry{.index = 23, .value = SetEntityDataPacket::IntValue{.value = collarColor}});
    }
    addRegistryMetadata(into, data, "sound_variant", 24, registryIds({"classic", "royal"}),
                        RegistryHolder::CatSoundVariant);
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
    // Read as a number rather than as a float tag: a save written before Health became one, or an
    // imported world, still has to say how alive the mob is. Zero here is a mob the client lays on
    // its side and plays the death animation for, however well it is walking around.
    if (data.contains("Health")) {
        entries.push_back(Entry{.index = 9,
                                .value = SetEntityDataPacket::FloatValue{
                                    .value = static_cast<float>(entityNbt::numericDouble(data, "Health"))}});
    }
    if (mobs::isMob(wire::common::Identifier::vanilla(std::string{type}))) {
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
        entries.push_back(
            Entry{.index = ageableMobs::kBabyDataIndex, .value = SetEntityDataPacket::BoolValue{.value = true}});
    }
    if (cubeMobs::isCube(type) && data.getInt("Size") != 0) {
        entries.push_back(Entry{.index = kCubeSizeDataIndex,
                                .value = SetEntityDataPacket::IntValue{.value = data.getInt("Size") + 1}});
    }

    if (type == "armor_stand") {
        addArmorStandMetadata(entries, data);
    } else if (type == "axolotl") {
        addIntIfSet(entries, data, "Variant", 18);
    } else if (type == "cat") {
        addCatMetadata(entries, data);
    } else if (type == "chicken") {
        addRegistryMetadata(entries, data, "variant", 18, registryIds({"temperate", "warm", "cold"}),
                            RegistryHolder::ChickenVariant);
        addRegistryMetadata(entries, data, "sound_variant", 19, registryIds({"classic", "picky"}),
                            RegistryHolder::ChickenSoundVariant);
    } else if (type == "cow") {
        addRegistryMetadata(entries, data, "variant", 18, registryIds({"temperate", "warm", "cold"}),
                            RegistryHolder::CowVariant);
        addRegistryMetadata(entries, data, "sound_variant", 19, registryIds({"classic", "moody"}),
                            RegistryHolder::CowSoundVariant);
    } else if (type == "fox") {
        if (data.getString("Type") == "snow") {
            entries.push_back(Entry{.index = 19, .value = SetEntityDataPacket::IntValue{.value = 1}});
        }
    } else if (type == "frog") {
        addRegistryMetadata(entries, data, "variant", 18, registryIds({"temperate", "warm", "cold"}),
                            RegistryHolder::FrogVariant);
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
        // parity: `Panda`'s own fields start at 18 (`UNHAPPY_COUNTER`, `SNEEZE_COUNTER`,
        // `EAT_COUNTER`), so the genes are 21 and 22. At 19 and 20 the client reads a byte where it
        // keeps an integer and drops the connection with a packet handling error.
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
        addRegistryMetadata(entries, data, "variant", 19, registryIds({"temperate", "warm", "cold"}),
                            RegistryHolder::PigVariant);
        addRegistryMetadata(entries, data, "sound_variant", 20, registryIds({"classic", "big", "mini"}),
                            RegistryHolder::PigSoundVariant);
    } else if (type == "rabbit") {
        addIntIfSet(entries, data, "RabbitType", 18);
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
        addRegistryMetadata(
            entries, data, "variant", 23,
            registryIds({"pale", "spotted", "snowy", "black", "ashen", "rusty", "woods", "chestnut", "striped"}),
            RegistryHolder::WolfVariant);
        addRegistryMetadata(entries, data, "sound_variant", 24,
                            registryIds({"classic", "puglin", "sad", "angry", "grumpy", "big", "cute"}),
                            RegistryHolder::WolfSoundVariant);
    } else if (type == "zombie_nautilus") {
        addRegistryMetadata(entries, data, "variant", 21, registryIds({"temperate", "warm"}),
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
    if (horseFlags.has_value() && (previous == nullptr || previous->horseFlags != horseFlags) &&
        (previous != nullptr || *horseFlags != 0)) {
        entries.push_back(Entry{.index = kAbstractHorseFlagsDataIndex,
                                .value = SetEntityDataPacket::ByteValue{.value = *horseFlags}});
    }
    return entries;
}

RuntimeEntityMetadata runtimeEntityMetadata(const ServerEntity& entity) {
    RuntimeEntityMetadata live{.airSupply = entity.airSupply};
    if (ageableMobs::hasBabyMetadata(entity.type().path())) {
        live.baby = entity.age < 0;
    }
    if (cubeMobs::isCube(entity.type())) {
        live.cubeSize = entity.cubeSize;
    }
    if (abstractHorseTypes().contains(entity.type().path())) {
        live.horseFlags = entity.tamedHorse ? kAbstractHorseTameFlag : std::int8_t{0};
    }
    return live;
}

} // namespace basalt::server::runtime::entity
