#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/VanillaGameData.hpp>
#include <basalt/server/runtime/plugins/PluginRecipeNbt.hpp>
#include <basalt/server/runtime/plugins/ServerInventories.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/PotionEffects.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/world/crafting/RecipeJson.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

namespace basalt::server::runtime::plugins {
namespace {
namespace reg = server::registry;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

[[nodiscard]] std::optional<Identifier> parsed(std::string_view id) {
    std::expected<Identifier, wire::common::IdentifierError> result = Identifier::parse(id);
    return result.has_value() ? std::optional{std::move(*result)} : std::nullopt;
}

[[nodiscard]] std::vector<std::uint8_t> encoded(NbtCompound compound) {
    return wire::nbt::nbtIO::writeAnonymous(NbtTag::ofCompound(std::move(compound)));
}
} // namespace

std::optional<std::vector<std::uint8_t>> ServerInventories::itemData(std::string_view id) const {
    const std::optional<Identifier> item = parsed(id);
    if (!item.has_value()) {
        return std::nullopt;
    }
    NbtCompound data;
    data.putInt("max_stack_size", reg::itemStacking::maxStackSize(*item));
    data.putInt("use_cooldown_ticks", reg::itemStacking::cooldownTicks(*item));
    data.putInt("max_damage", reg::itemDurability::maxDamageOf(*item));
    data.putInt("burn_ticks", reg::fuels::burnTicks(*item));
    if (const Identifier block = reg::itemBlocks::of(*item); block != *item) {
        data.putString("block", block.toString());
    }
    if (const Identifier* remainder = reg::craftingRemainders::of(*item); remainder != nullptr) {
        data.putString("crafting_remainder", remainder->toString());
    }
    if (const reg::FoodValue* food = reg::itemFood::of(*item); food != nullptr) {
        NbtCompound edible;
        edible.putInt("nutrition", food->nutrition);
        edible.putFloat("saturation", food->saturation);
        edible.putBoolean("can_always_eat", food->canAlwaysEat);
        edible.putFloat("consume_seconds", food->consumeSeconds);
        if (food->remainder.has_value()) {
            edible.putString("remainder", food->remainder->toString());
        }
        data.set("food", NbtTag::ofCompound(std::move(edible)));
    }
    if (const reg::ArmourPiece* armour = reg::itemArmour::of(*item); armour != nullptr) {
        NbtCompound piece;
        piece.putString("slot", std::string{armour->slot});
        piece.putFloat("armor", armour->armour);
        piece.putFloat("toughness", armour->toughness);
        piece.putFloat("knockback_resistance", armour->knockbackResistance);
        data.set("armor", NbtTag::ofCompound(std::move(piece)));
    }
    if (const reg::WeaponStats* weapon = reg::itemWeapons::of(*item); weapon != nullptr) {
        NbtCompound stats;
        stats.putDouble("attack_damage", weapon->attackDamage);
        stats.putDouble("attack_speed", weapon->attackSpeed);
        data.set("weapon", NbtTag::ofCompound(std::move(stats)));
    }
    if (const std::span<const reg::itemTools::Rule> rules = reg::itemTools::rulesFor(*item); !rules.empty()) {
        NbtCompound tool;
        std::vector<NbtTag> entries;
        bool sword = false;
        for (const reg::itemTools::Rule& rule : rules) {
            NbtCompound entry;
            entry.putString("blocks", "#" + std::string{rule.blockTag});
            if (rule.speed.has_value()) {
                entry.putFloat("speed", *rule.speed);
            }
            entry.putBoolean("correct_for_drops", rule.correctForDrops);
            sword = sword || rule.blockTag == "minecraft:sword_instantly_mines";
            entries.push_back(NbtTag::ofCompound(std::move(entry)));
        }
        tool.set("rules", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(entries)}));
        // parity: `Items.swordTool` and the mace cost two durability a block and cannot dig in creative.
        tool.putInt("damage_per_block", sword ? 2 : 1);
        tool.putBoolean("can_destroy_blocks_in_creative", !sword);
        data.set("tool", NbtTag::ofCompound(std::move(tool)));
    }
    if (const std::optional<std::int32_t> chance = world::composters::chanceOf(*item); chance.has_value()) {
        data.putInt("composter_chance", *chance);
    }
    return encoded(std::move(data));
}

std::optional<std::vector<std::uint8_t>> ServerInventories::enchantmentData(std::string_view id) const {
    const std::optional<Identifier> enchantment = parsed(id);
    if (!enchantment.has_value() || !data_) {
        return std::nullopt;
    }
    const NbtCompound* definition = data_().enchantments.definitionOf(*enchantment);
    return definition == nullptr ? std::nullopt : std::optional{encoded(*definition)};
}

std::optional<std::vector<std::uint8_t>> ServerInventories::mobEffects() const {
    std::vector<NbtTag> entries;
    for (const Identifier& id : world::mobEffects::ids()) {
        NbtCompound entry;
        entry.putString("id", id.toString());
        entry.putInt("protocol_id", world::mobEffects::protocolIdOf(id).value_or(0));
        entry.putInt("color", world::mobEffects::colorOf(id).value_or(0));
        entries.push_back(NbtTag::ofCompound(std::move(entry)));
    }
    NbtCompound root;
    root.set("effects", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(entries)}));
    return encoded(std::move(root));
}

std::optional<std::vector<std::uint8_t>> ServerInventories::potions() const {
    std::vector<NbtTag> entries;
    for (const Identifier& id : world::potionEffects::ids()) {
        NbtCompound entry;
        entry.putString("id", id.toString());
        std::vector<NbtTag> effects;
        for (const world::ActiveEffect& effect : world::potionEffects::ofPotion(id.path())) {
            NbtCompound item;
            item.putString("id", effect.effect.toString());
            item.putInt("amplifier", effect.amplifier);
            item.putInt("duration", effect.remainingTicks);
            effects.push_back(NbtTag::ofCompound(std::move(item)));
        }
        entry.set("effects", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(effects)}));
        entries.push_back(NbtTag::ofCompound(std::move(entry)));
    }
    NbtCompound root;
    root.set("potions", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(entries)}));
    return encoded(std::move(root));
}

std::vector<std::string> ServerInventories::enchantmentIds() const {
    std::vector<std::string> ids;
    if (data_) {
        for (const Identifier& id : data_().enchantments.ids()) {
            ids.push_back(id.toString());
        }
    }
    return ids;
}

std::optional<std::vector<std::uint8_t>> ServerInventories::craft(std::int32_t width, std::int32_t height,
                                                                  std::span<const std::uint8_t> items) const {
    if (!data_ || width <= 0 || height <= 0) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(items);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    const wire::nbt::NbtList* list = root == nullptr ? nullptr : root->getList("items");
    if (list == nullptr) {
        return std::nullopt;
    }
    std::vector<std::optional<server::world::loot::ItemStack>> cells;
    for (const NbtTag& cell : *list) {
        const NbtCompound* compound = cell.get<NbtCompound>();
        std::optional<server::world::loot::ItemStack> stack;
        if (compound != nullptr && compound->contains("id")) {
            const std::expected<Identifier, wire::common::IdentifierError> id =
                Identifier::parse(compound->getString("id"));
            if (id.has_value()) {
                stack = server::world::loot::ItemStack{.item = *id, .count = compound->getInt("count", 1)};
                if (const NbtCompound* components = compound->getCompound("components"); components != nullptr) {
                    stack->components = *components;
                }
            }
        }
        cells.push_back(std::move(stack));
    }
    cells.resize(static_cast<std::size_t>(width * height));
    const server::world::crafting::CraftingInput input =
        server::world::crafting::CraftingInput::of(width, height, cells);
    const std::optional<world::RealRecipes::Match> match = data_().recipes.recipeMatching(input);
    if (!match.has_value()) {
        return std::nullopt;
    }
    const auto stackToNbt = [](const server::world::loot::ItemStack& stack) {
        NbtCompound out;
        out.putString("id", stack.item.toString());
        out.putInt("count", stack.count);
        out.set("components", NbtTag::ofCompound(stack.components));
        return out;
    };
    NbtCompound out;
    out.putString("recipe", match->id.toString());
    out.set("result", NbtTag::ofCompound(stackToNbt(match->recipe->assemble(input))));
    std::vector<NbtTag> remaining;
    for (const std::optional<server::world::loot::ItemStack>& left : match->recipe->remainingItems(input)) {
        remaining.push_back(NbtTag::ofCompound(left.has_value() ? stackToNbt(*left) : NbtCompound{}));
    }
    out.set("remaining", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(remaining)}));
    return encoded(std::move(out));
}

std::vector<std::string> ServerInventories::recipeIds() const {
    std::vector<std::string> ids;
    if (data_) {
        for (const Identifier& id : data_().recipes.listed()) {
            ids.push_back(id.toString());
        }
    }
    return ids;
}

std::optional<std::vector<std::uint8_t>> ServerInventories::recipeData(std::string_view id) const {
    const std::optional<Identifier> parsedId = parsed(id);
    if (!parsedId.has_value() || !data_) {
        return std::nullopt;
    }
    const server::world::crafting::Recipe* recipe = data_().recipes.recipeFor(*parsedId);
    return recipe == nullptr ? std::nullopt : std::optional{encoded(recipeToNbt(*recipe))};
}

bool ServerInventories::addRecipe(std::string_view id, std::span<const std::uint8_t> data) {
    const std::optional<Identifier> parsedId = parsed(id);
    if (!parsedId.has_value() || !data_) {
        return false;
    }
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(data);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    std::optional<server::world::crafting::Recipe> recipe = root == nullptr ? std::nullopt : recipeFromNbt(*root);
    return recipe.has_value() && data_().recipes.add(*parsedId, std::move(*recipe));
}

bool ServerInventories::addRecipeJson(std::string_view id, std::string_view json) {
    const std::optional<Identifier> parsedId = parsed(id);
    if (!parsedId.has_value() || !data_) {
        return false;
    }
    const world::realItemTags::TagMembers& tags = data_().itemTags;
    const server::world::crafting::recipeJson::TagResolver resolve = [&tags](std::string_view name) {
        std::set<std::string, std::less<>> members;
        for (const Identifier& member : world::realItemTags::membersOf(tags, name)) {
            members.insert(member.toString());
        }
        return members;
    };
    std::expected<server::world::crafting::Recipe, server::world::crafting::RecipeParseError> recipe =
        server::world::crafting::recipeJson::parse(json, resolve);
    return recipe.has_value() && data_().recipes.add(*parsedId, std::move(*recipe));
}

bool ServerInventories::removeRecipe(std::string_view id) {
    const std::optional<Identifier> parsedId = parsed(id);
    return parsedId.has_value() && data_ && data_().recipes.remove(*parsedId);
}

void ServerInventories::clearRecipes() {
    if (data_) {
        data_().recipes.clear();
    }
}

void ServerInventories::resetRecipes() {
    if (data_) {
        data_().recipes.restore();
    }
}

std::vector<std::string> ServerInventories::registryTags(std::string_view registry) const {
    std::vector<std::string> flat;
    if (registry != "enchantment" || !data_) {
        return flat;
    }
    for (const auto& [tag, members] : data_().enchantmentTags) {
        std::string joined;
        for (const Identifier& member : members) {
            joined += joined.empty() ? member.toString() : "|" + member.toString();
        }
        flat.push_back(tag.toString());
        flat.push_back(std::move(joined));
    }
    return flat;
}

} // namespace basalt::server::runtime::plugins