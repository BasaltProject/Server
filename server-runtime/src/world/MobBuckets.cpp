#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/MobBuckets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::mobBuckets {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kEntityData = "minecraft:bucket_entity_data";
constexpr std::string_view kCustomName = "minecraft:custom_name";
constexpr std::string_view kAxolotlVariant = "minecraft:axolotl/variant";
constexpr std::string_view kSalmonSize = "minecraft:salmon/size";
constexpr std::string_view kTropicalPattern = "minecraft:tropical_fish/pattern";
constexpr std::string_view kTropicalBaseColor = "minecraft:tropical_fish/base_color";
constexpr std::string_view kTropicalPatternColor = "minecraft:tropical_fish/pattern_color";
constexpr std::string_view kSulfurContent = "minecraft:sulfur_cube_content";

constexpr std::array<Kind, 7> kKinds{{
    {"cod_bucket", "cod", "item.bucket.fill_fish", "item.bucket.empty_fish", true},
    {"salmon_bucket", "salmon", "item.bucket.fill_fish", "item.bucket.empty_fish", true},
    {"pufferfish_bucket", "pufferfish", "item.bucket.fill_fish", "item.bucket.empty_fish", true},
    {"tropical_fish_bucket", "tropical_fish", "item.bucket.fill_fish", "item.bucket.empty_fish", true},
    {"axolotl_bucket", "axolotl", "item.bucket.fill_axolotl", "item.bucket.empty_axolotl", true},
    {"tadpole_bucket", "tadpole", "item.bucket.fill_tadpole", "item.bucket.empty_tadpole", true},
    {"sulfur_cube_bucket", "sulfur_cube", "item.bucket.fill_sulfur_cube", "item.bucket.empty_sulfur_cube", false},
}};

constexpr std::array<std::string_view, 5> kAxolotlVariants{"lucy", "wild", "gold", "cyan", "blue"};
constexpr std::array<std::string_view, 3> kSalmonSizes{"small", "medium", "large"};
constexpr std::array<std::string_view, 16> kDyes{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                 "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                 "brown", "green",  "red",        "black"};
constexpr std::array<std::pair<std::string_view, std::int32_t>, 12> kPatterns{{{"kob", 0x000},
                                                                               {"sunstreak", 0x100},
                                                                               {"snooper", 0x200},
                                                                               {"dasher", 0x300},
                                                                               {"brinely", 0x400},
                                                                               {"spotty", 0x500},
                                                                               {"flopper", 0x001},
                                                                               {"stripey", 0x101},
                                                                               {"glitter", 0x201},
                                                                               {"blockfish", 0x301},
                                                                               {"betty", 0x401},
                                                                               {"clayfish", 0x501}}};

constexpr std::array<std::string_view, 6> kFlags{"NoAI",    "Silent",       "NoGravity",
                                                 "Glowing", "Invulnerable", "PersistenceRequired"};

[[nodiscard]] std::string_view dyeName(std::int32_t id) {
    return kDyes[static_cast<std::size_t>(id >= 0 && id < 16 ? id : 0)];
}

[[nodiscard]] std::int32_t dyeId(const NbtCompound& components, std::string_view key) {
    const NbtTag* tag = components.find(key);
    const std::string* name = tag == nullptr ? nullptr : tag->get<std::string>();
    const auto found = name == nullptr ? kDyes.end() : std::ranges::find(kDyes, *name);
    return found == kDyes.end() ? 0 : static_cast<std::int32_t>(found - kDyes.begin());
}

[[nodiscard]] std::string_view patternName(std::int32_t packed) {
    const auto found = std::ranges::find_if(kPatterns, [packed](const auto& one) { return one.second == packed; });
    return found == kPatterns.end() ? kPatterns.front().first : found->first;
}

template <std::size_t N>
[[nodiscard]] std::int32_t indexIn(const std::array<std::string_view, N>& names, const NbtCompound& components,
                                   std::string_view key, std::int32_t fallback) {
    const NbtTag* tag = components.find(key);
    const std::string* name = tag == nullptr ? nullptr : tag->get<std::string>();
    const auto found = name == nullptr ? names.end() : std::ranges::find(names, *name);
    return found == names.end() ? fallback : static_cast<std::int32_t>(found - names.begin());
}

[[nodiscard]] NbtTag positionOf(double x, double y, double z) {
    NbtList list{NbtType::Double};
    list.push(NbtTag::ofDouble(x));
    list.push(NbtTag::ofDouble(y));
    list.push(NbtTag::ofDouble(z));
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] NbtTag rotationOf(float yaw) {
    NbtList list{NbtType::Float};
    list.push(NbtTag::ofFloat(yaw));
    list.push(NbtTag::ofFloat(0.0F));
    return NbtTag::ofList(std::move(list));
}

} // namespace

const Kind* ofItem(const Identifier& item) {
    if (item.namespaceName() != Identifier::kVanillaNamespace) {
        return nullptr;
    }
    const auto found = std::ranges::find_if(kKinds, [&item](const Kind& kind) { return kind.item == item.path(); });
    return found == kKinds.end() ? nullptr : std::to_address(found);
}

const Kind* ofEntity(const Identifier& type) {
    if (type.namespaceName() != Identifier::kVanillaNamespace) {
        return nullptr;
    }
    const auto found = std::ranges::find_if(kKinds, [&type](const Kind& kind) { return kind.entity == type.path(); });
    return found == kKinds.end() ? nullptr : std::to_address(found);
}

bool pickedUpWith(const Kind& kind, const Identifier& held) {
    return held.namespaceName() == Identifier::kVanillaNamespace &&
           held.path() == (kind.water ? "water_bucket" : "bucket");
}

bool isFish(const Identifier& type) {
    const std::string_view path = type.path();
    return path == "cod" || path == "salmon" || path == "pufferfish" || path == "tropical_fish";
}

std::optional<std::int32_t> tropicalPatternId(std::string_view name) {
    const auto found = std::ranges::find_if(kPatterns, [name](const auto& one) { return one.first == name; });
    return found == kPatterns.end() ? std::nullopt : std::optional{found->second};
}

storage::StoredItemStack bucketOf(const Kind& kind, const entity::ServerEntity& mob) {
    const NbtCompound saved = entity::entityNbt::encode(mob);
    NbtCompound data;
    NbtCompound components;
    for (const std::string_view flag : kFlags) {
        if (saved.getBoolean(flag)) {
            data.putBoolean(flag, true);
        }
    }
    data.putFloat("Health", saved.getFloat("Health"));
    if (mob.customName.has_value()) {
        components.set(kCustomName, NbtTag::ofString(*mob.customName));
    }
    const std::string_view path = kind.entity;
    if (path == "axolotl" || path == "tadpole") {
        data.putInt("Age", entity::entityNbt::numericInt(saved, "Age"));
        data.putBoolean("AgeLocked", mob.ageLocked);
    }
    if (path == "axolotl") {
        const std::int32_t variant = std::clamp(entity::entityNbt::numericInt(saved, "Variant"), 0, 4);
        components.set(kAxolotlVariant,
                       NbtTag::ofString(std::string{kAxolotlVariants[static_cast<std::size_t>(variant)]}));
    } else if (path == "salmon") {
        const std::string_view size = saved.getString("type", "medium");
        const bool known = std::ranges::find(kSalmonSizes, size) != kSalmonSizes.end();
        components.set(kSalmonSize, NbtTag::ofString(std::string{known ? size : "medium"}));
    } else if (path == "tropical_fish") {
        const std::int32_t packed = entity::entityNbt::numericInt(saved, "Variant");
        components.set(kTropicalPattern, NbtTag::ofString(std::string{patternName(packed & 0xFFFF)}));
        components.set(kTropicalBaseColor, NbtTag::ofString(std::string{dyeName((packed >> 16) & 0xFF)}));
        components.set(kTropicalPatternColor, NbtTag::ofString(std::string{dyeName((packed >> 24) & 0xFF)}));
    } else if (path == "sulfur_cube") {
        data.putInt("age", mob.age);
        data.putBoolean("age_locked", mob.ageLocked);
        if (const auto body = entity::mobEquipment::get(mob, entity::mobEquipment::Slot::Body); body.has_value()) {
            NbtCompound content;
            content.putString("id", body->item.toString());
            content.putInt("count", body->count);
            if (body->components.size() != 0) {
                content.set("components", NbtTag::ofCompound(body->components));
            }
            components.set(kSulfurContent, NbtTag::ofCompound(std::move(content)));
        }
    }
    components.set(kEntityData, NbtTag::ofCompound(std::move(data)));
    return storage::StoredItemStack{
        .id = "minecraft:" + std::string{kind.item}, .count = 1, .components = std::move(components)};
}

std::unique_ptr<entity::ServerEntity> spawnFrom(const Kind& kind, const NbtCompound& components, std::int32_t entityId,
                                                std::int32_t protocolType, double x, double y, double z, float yaw) {
    static const NbtCompound kNone;
    const NbtCompound* stored = components.getCompound(kEntityData);
    const NbtCompound& data = stored != nullptr ? *stored : kNone;
    NbtCompound tag;
    tag.putString("id", "minecraft:" + std::string{kind.entity});
    tag.set("Pos", positionOf(x, y, z));
    tag.set("Rotation", rotationOf(yaw));
    for (const std::string_view flag : kFlags) {
        if (data.getBoolean(flag)) {
            tag.putBoolean(flag, true);
        }
    }
    if (data.contains("Health")) {
        tag.putFloat("Health", data.getFloat("Health"));
    }
    if (const NbtTag* name = components.find(kCustomName); name != nullptr) {
        tag.putString("CustomName", wire::text::toPlainText(wire::text::componentNbt::deserialize(*name)));
    }
    const std::string_view path = kind.entity;
    bool ageLocked = false;
    if (path == "axolotl" || path == "tadpole") {
        tag.putInt("Age", data.getInt("Age"));
        ageLocked = data.getBoolean("AgeLocked");
    }
    if (path == "axolotl") {
        tag.putInt("Variant", indexIn(kAxolotlVariants, components, kAxolotlVariant, 0));
        tag.putBoolean("FromBucket", true);
    } else if (path == "salmon") {
        tag.putString(
            "type",
            std::string{kSalmonSizes[static_cast<std::size_t>(indexIn(kSalmonSizes, components, kSalmonSize, 1))]});
        tag.putBoolean("FromBucket", true);
    } else if (path == "tropical_fish") {
        const NbtTag* pattern = components.find(kTropicalPattern);
        const std::string* name = pattern == nullptr ? nullptr : pattern->get<std::string>();
        const std::int32_t packedPattern = name == nullptr ? 0 : tropicalPatternId(*name).value_or(0);
        tag.putInt("Variant", packedPattern | (dyeId(components, kTropicalBaseColor) << 16) |
                                  (dyeId(components, kTropicalPatternColor) << 24));
        tag.putBoolean("FromBucket", true);
    } else if (path == "cod" || path == "pufferfish" || path == "tadpole") {
        tag.putBoolean("FromBucket", true);
    } else if (path == "sulfur_cube") {
        tag.putBoolean("from_bucket", true);
        ageLocked = data.getBoolean("age_locked");
        if (const NbtCompound* content = components.getCompound(kSulfurContent); content != nullptr) {
            NbtCompound equipment;
            equipment.set("body", NbtTag::ofCompound(*content));
            tag.set("equipment", NbtTag::ofCompound(std::move(equipment)));
        }
    }
    std::unique_ptr<entity::ServerEntity> spawned = entity::entityNbt::decode(tag, entityId, protocolType);
    if (spawned != nullptr) {
        spawned->ageLocked = ageLocked;
        if (path == "sulfur_cube") {
            spawned->age = data.getInt("age");
        }
    }
    return spawned;
}

} // namespace basalt::server::runtime::world::mobBuckets
