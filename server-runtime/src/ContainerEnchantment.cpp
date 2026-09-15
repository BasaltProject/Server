#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Enchanting.hpp>
#include <basalt/server/runtime/world/ItemEnchantability.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace itemComponents = runtime::item::itemComponents;

using player::Menu;
using player::PlayerSession;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ContainerSetDataPacket;
using wire::protocol::play::OpenScreenPacket;
using wire::protocol::play::SoundSource;
using wire::text::Component;

/// parity: the three rows, which is what the screen draws and nothing more.
constexpr std::size_t kRows = 3;

[[nodiscard]] bool alreadyEnchanted(const storage::StoredItemStack& stack) {
    return stack.components.find(itemComponents::kEnchantments) != nullptr ||
           stack.components.find(itemComponents::kStoredEnchantments) != nullptr;
}

} // namespace

void ContainerMenuController::openEnchantmentMenu(PlayerSession& session, const BlockPos& pos) {
    returnCraftingGrid(session);
    openMenu = Menu::enchantment(allocateContainerId());
    openBlock = pos;
    openBlockSecond.reset();
    openEnderChest_ = false;
    openStonecutter_ = false;
    openSmithing_ = false;
    openAnvil_ = false;
    openGrindstone_ = false;
    openEnchantment_ = true;
    std::ranges::fill(enchantmentSlots_, std::nullopt);
    enchantmentCosts_ = {0, 0, 0};
    enchantmentClues_ = {-1, -1, -1};
    enchantmentLevels_ = {-1, -1, -1};
    if (session.enchantmentSeed == 0 && options_.nextSeed) {
        session.enchantmentSeed = static_cast<std::int32_t>(options_.nextSeed());
    }
    openBlockSize = static_cast<std::int32_t>(enchantmentSlots_.size());
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kEnchantment,
                         .title = Component::translatable("container.enchant")}
            .write(buffer);
    });
    sendInventory();
    sendEnchantmentNumbers();
}

void ContainerMenuController::rollEnchantmentOffers() {
    PlayerSession* session = owner_->playerSession();
    const std::optional<storage::StoredItemStack>& input = enchantmentSlots_[Menu::kEnchantmentInput];
    enchantmentCosts_ = {0, 0, 0};
    enchantmentClues_ = {-1, -1, -1};
    enchantmentLevels_ = {-1, -1, -1};
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr || !openBlock.has_value() || !input.has_value() ||
        !world::itemEnchantability::valueOf(input->id).has_value() || alreadyEnchanted(*input)) {
        sendEnchantmentNumbers();
        return;
    }
    // parity: `EnchantingTableBlock.isValidBookShelf`: a shelf two out with nothing between it and
    // the table.
    std::int32_t shelves = 0;
    for (const std::array<std::int32_t, 3>& offset : world::enchanting::bookshelfOffsets()) {
        const std::optional<std::int32_t> shelf =
            world->state().blockState(openBlock->x + offset[0], openBlock->y + offset[1], openBlock->z + offset[2]);
        const std::optional<std::int32_t> between = world->state().blockState(
            openBlock->x + (offset[0] / 2), openBlock->y + offset[1], openBlock->z + (offset[2] / 2));
        const Identifier* shelfBlock =
            shelf.has_value() ? server::registry::blockStateDefinitions::blockOf(*shelf) : nullptr;
        const Identifier* betweenBlock =
            between.has_value() ? server::registry::blockStateDefinitions::blockOf(*between) : nullptr;
        const world::realBlockTags::TagMembers* tags = owner_->options().blockTags;
        if (shelfBlock == nullptr || betweenBlock == nullptr || tags == nullptr) {
            continue;
        }
        if (world::realBlockTags::contains(*tags, "minecraft:enchantment_power_provider", *shelfBlock) &&
            world::realBlockTags::contains(*tags, "minecraft:enchantment_power_transmitter", *betweenBlock)) {
            ++shelves;
        }
    }

    const world::EnchantmentBooks books{.enchantments = owner_->options().enchantments,
                                        .itemTags = owner_->options().itemTags,
                                        .enchantmentTags = owner_->options().enchantmentTags};
    random::RandomSource random = random::RandomSource::legacy(session->enchantmentSeed);
    for (std::size_t row = 0; row < kRows; ++row) {
        enchantmentCosts_[row] = world::enchanting::costFor(random, static_cast<std::int32_t>(row), shelves);
        if (enchantmentCosts_[row] < static_cast<std::int32_t>(row) + 1) {
            enchantmentCosts_[row] = 0;
        }
    }
    for (std::size_t row = 0; row < kRows; ++row) {
        if (enchantmentCosts_[row] <= 0) {
            continue;
        }
        random::RandomSource rowRandom =
            random::RandomSource::legacy(session->enchantmentSeed + static_cast<std::int64_t>(row));
        std::vector<world::enchanting::Offer> offers =
            world::enchanting::select(rowRandom, books, *input, enchantmentCosts_[row]);
        // parity: a plain book is offered one fewer than everything else, drawn out at random.
        if (input->id == "minecraft:book" && offers.size() > 1) {
            offers.erase(offers.begin() + rowRandom.nextInt(static_cast<std::int32_t>(offers.size())));
        }
        if (offers.empty()) {
            continue;
        }
        const world::enchanting::Offer& shown =
            offers[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(offers.size())))];
        const std::optional<std::int32_t> id =
            books.enchantments == nullptr ? std::nullopt : books.enchantments->protocolIdOf(shown.enchantment);
        enchantmentClues_[row] = id.value_or(-1);
        enchantmentLevels_[row] = shown.level;
    }
    sendEnchantmentNumbers();
}

void ContainerMenuController::takeEnchantmentOffer(PlayerSession& session, const std::int32_t row) {
    if (row < 0 || static_cast<std::size_t>(row) >= kRows || !openEnchantment_) {
        return;
    }
    const std::int32_t price = row + 1;
    const bool free = session.gamemode == wire::protocol::play::Gamemode::Creative;
    std::optional<storage::StoredItemStack>& input = enchantmentSlots_[Menu::kEnchantmentInput];
    std::optional<storage::StoredItemStack>& lapis = enchantmentSlots_[Menu::kEnchantmentLapis];
    // parity: `clickMenuButton`: the lapis, the levels and a row that is offering anything at all.
    if (!free && (!lapis.has_value() || lapis->count < price)) {
        return;
    }
    if (enchantmentCosts_[static_cast<std::size_t>(row)] <= 0 || !input.has_value()) {
        return;
    }
    if (!free && (session.experience.level() < price ||
                  session.experience.level() < enchantmentCosts_[static_cast<std::size_t>(row)])) {
        return;
    }
    const world::EnchantmentBooks books{.enchantments = owner_->options().enchantments,
                                        .itemTags = owner_->options().itemTags,
                                        .enchantmentTags = owner_->options().enchantmentTags};
    random::RandomSource random =
        random::RandomSource::legacy(session.enchantmentSeed + static_cast<std::int64_t>(row));
    std::vector<world::enchanting::Offer> offers =
        world::enchanting::select(random, books, *input, enchantmentCosts_[static_cast<std::size_t>(row)]);
    if (input->id == "minecraft:book" && offers.size() > 1) {
        offers.erase(offers.begin() + random.nextInt(static_cast<std::int32_t>(offers.size())));
    }
    if (offers.empty()) {
        return;
    }
    // parity: a plain book becomes an enchanted one, and what goes on it is stored rather than worn.
    storage::StoredItemStack made = *input;
    if (made.id == "minecraft:book") {
        made.id = "minecraft:enchanted_book";
    }
    const std::string_view component =
        made.id == "minecraft:enchanted_book" ? itemComponents::kStoredEnchantments : itemComponents::kEnchantments;
    NbtCompound levels;
    if (const NbtCompound* existing = made.components.getCompound(component); existing != nullptr) {
        levels = *existing;
    }
    for (const world::enchanting::Offer& offer : offers) {
        const std::string name = offer.enchantment.toString();
        levels.putInt(name, std::max(levels.contains(name) ? levels.getInt(name) : 0, offer.level));
    }
    made.components.set(component, NbtTag::ofCompound(std::move(levels)));
    input = made;

    if (!free) {
        session.experience.giveLevels(-price);
        if (session.connection().syncExperience) {
            session.connection().syncExperience();
        }
        if (lapis.has_value()) {
            lapis->count -= price;
            if (lapis->count <= 0) {
                lapis.reset();
            }
        }
    }
    session.stats.increment(storage::stats::enchantItem());
    // parity: the seed moves on, which is what gives the next item different offers.
    if (options_.nextSeed) {
        session.enchantmentSeed = static_cast<std::int32_t>(options_.nextSeed());
    }
    rollEnchantmentOffers();
    sendInventory();
    world::LoadedWorld* world = owner_->currentWorld();
    if (world != nullptr && openBlock.has_value()) {
        owner_->blockInteraction().broadcastEffects(
            *openBlock, world->state().blockState(openBlock->x, openBlock->y, openBlock->z).value_or(0),
            {world::blockEffect::Sound{
                .name = "block.enchantment_table.use", .source = SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F}});
    }
}

void ContainerMenuController::sendEnchantmentNumbers() {
    if (!openEnchantment_) {
        return;
    }
    const PlayerSession* session = owner_->playerSession();
    const auto send = [this](const std::int16_t id, const std::int32_t value) {
        owner_->sendPlayPacket(ContainerSetDataPacket::kId, [this, id, value](wire::io::ByteBuffer& buffer) {
            ContainerSetDataPacket{
                .containerId = openMenu.containerId(), .id = id, .value = static_cast<std::int16_t>(value)}
                .write(buffer);
        });
    };
    for (std::size_t row = 0; row < kRows; ++row) {
        const auto at = static_cast<std::int16_t>(row);
        send(ContainerSetDataPacket::kEnchantmentCost + at, enchantmentCosts_[row]);
        send(ContainerSetDataPacket::kEnchantmentClue + at, enchantmentClues_[row]);
        send(ContainerSetDataPacket::kEnchantmentLevelClue + at, enchantmentLevels_[row]);
    }
    send(ContainerSetDataPacket::kEnchantmentSeed, session == nullptr ? 0 : session->enchantmentSeed);
}

} // namespace basalt::server::runtime
