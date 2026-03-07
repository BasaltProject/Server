#include <basalt/server/runtime/world/Looms.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace basalt::server::runtime::world::looms {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kComponent = "minecraft:banner_patterns";
constexpr std::string_view kDyeSuffix = "_dye";
constexpr std::string_view kPatternSuffix = "_banner_pattern";
constexpr std::string_view kBannerSuffix = "_banner";

/// parity: the sixteen of `DyeColor`, which is what the `dyes` item tag holds.
constexpr std::array<std::string_view, 16> kColours{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                    "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                    "brown", "green",  "red",        "black"};

[[nodiscard]] std::string_view withoutNamespace(std::string_view item) {
    const std::size_t colon = item.find(':');
    return colon == std::string_view::npos ? item : item.substr(colon + 1);
}

} // namespace

std::optional<std::string> dyeColourOf(std::string_view item) {
    const std::string_view name = withoutNamespace(item);
    if (!name.ends_with(kDyeSuffix)) {
        return std::nullopt;
    }
    const std::string_view colour = name.substr(0, name.size() - kDyeSuffix.size());
    if (std::ranges::find(kColours, colour) == kColours.end()) {
        return std::nullopt;
    }
    return std::string{colour};
}

std::optional<std::string> patternTagOf(std::string_view item) {
    const std::string_view name = withoutNamespace(item);
    if (!name.ends_with(kPatternSuffix)) {
        return std::nullopt;
    }
    return "minecraft:pattern_item/" + std::string{name.substr(0, name.size() - kPatternSuffix.size())};
}

bool isBanner(std::string_view item) {
    const std::string_view name = withoutNamespace(item);
    if (!name.ends_with(kBannerSuffix) || name.ends_with(kPatternSuffix)) {
        return false;
    }
    return std::ranges::find(kColours, name.substr(0, name.size() - kBannerSuffix.size())) != kColours.end();
}

std::size_t layersOn(const storage::StoredItemStack& banner) {
    const NbtList* layers = banner.components.getList(kComponent);
    return layers == nullptr ? 0 : layers->size();
}

std::optional<storage::StoredItemStack> weave(const storage::StoredItemStack& banner, const std::string_view pattern,
                                              const std::string_view colour) {
    if (layersOn(banner) >= kMaxLayers) {
        return std::nullopt;
    }
    storage::StoredItemStack made = banner;
    made.count = 1;
    NbtList layers{NbtType::Compound};
    if (const NbtList* already = banner.components.getList(kComponent); already != nullptr) {
        for (const NbtTag& layer : *already) {
            layers.push(layer);
        }
    }
    NbtCompound added;
    added.putString("pattern", std::string{pattern});
    added.putString("color", std::string{colour});
    layers.push(NbtTag::ofCompound(std::move(added)));
    made.components.set(kComponent, NbtTag::ofList(std::move(layers)));
    return made;
}

} // namespace basalt::server::runtime::world::looms
