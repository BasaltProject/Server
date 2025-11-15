#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::world::shulkers {
namespace {

constexpr std::array<std::string_view, 17> kBlocks{
    "shulker_box",        "white_shulker_box",  "orange_shulker_box", "magenta_shulker_box", "light_blue_shulker_box",
    "yellow_shulker_box", "lime_shulker_box",   "pink_shulker_box",   "gray_shulker_box",    "light_gray_shulker_box",
    "cyan_shulker_box",   "purple_shulker_box", "blue_shulker_box",   "brown_shulker_box",   "green_shulker_box",
    "red_shulker_box",    "black_shulker_box"};

} // namespace

std::span<const std::string_view> blocks() { return kBlocks; }

bool isShulkerBox(std::string_view path) { return std::ranges::find(kBlocks, path) != kBlocks.end(); }

std::optional<wire::nbt::NbtCompound> contentsOf(const wire::nbt::NbtCompound& data) {
    const blockInventory::Slots slots = blockInventory::read(data, kSize);
    if (std::ranges::none_of(slots, [](const auto& slot) { return slot.has_value(); })) {
        return std::nullopt;
    }
    wire::nbt::NbtCompound carried;
    blockInventory::write(carried, slots);
    return carried;
}

void restore(wire::nbt::NbtCompound& data, const wire::nbt::NbtCompound& carried) {
    blockInventory::write(data, blockInventory::read(carried, kSize));
}

} // namespace basalt::server::runtime::world::shulkers
