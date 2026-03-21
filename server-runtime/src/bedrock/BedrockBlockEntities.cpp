#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/bedrock/BedrockBlockEntities.hpp>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

/// The dyes in the order every edition numbers them, which is what a bed's colour byte counts in.
constexpr std::array<std::string_view, 16> kDyes{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                 "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                 "brown", "green",  "red",        "black"};

/// Marks every state of one block with a colour. One block's states are laid out together, so the
/// run is found by asking each one who it belongs to until the answer changes.
void markStatesOf(const wire::common::Identifier& block, const std::int8_t colour, std::vector<std::int8_t>& out) {
    const auto first = server::registry::blocks::defaultStateIdOf(block);
    if (!first) {
        return;
    }
    for (auto state = *first; state < static_cast<std::int32_t>(out.size()); ++state) {
        const auto what = server::registry::blocks::byStateId(state);
        if (!what || what->block != block) {
            break;
        }
        out[static_cast<std::size_t>(state)] = colour;
    }
    for (auto state = *first - 1; state >= 0; --state) {
        const auto what = server::registry::blocks::byStateId(state);
        if (!what || what->block != block) {
            break;
        }
        out[static_cast<std::size_t>(state)] = colour;
    }
}

/// Every state's colour for one family of coloured blocks, by state id, built once. The world hands
/// out state ids by the million and asking the registry for each one's name is not something a
/// chunk can afford.
std::vector<std::int8_t> colourTable(const std::span<const std::string_view> suffixes) {
    std::vector<std::int8_t> out(static_cast<std::size_t>(server::registry::blocks::stateCount()), -1);
    for (std::size_t colour = 0; colour < kDyes.size(); ++colour) {
        for (const std::string_view suffix : suffixes) {
            markStatesOf(wire::common::Identifier::vanilla(std::string{kDyes[colour]} + std::string{suffix}),
                         static_cast<std::int8_t>(colour), out);
        }
    }
    return out;
}

const std::vector<std::int8_t>& bedColours() {
    static constexpr std::array<std::string_view, 1> kBeds{"_bed"};
    static const std::vector<std::int8_t> table = colourTable(kBeds);
    return table;
}

const std::vector<std::int8_t>& bannerColours() {
    // Both shapes carry it: over there a banner on a wall is one block and its colour is a tag too.
    static constexpr std::array<std::string_view, 2> kBanners{"_banner", "_wall_banner"};
    static const std::vector<std::int8_t> table = colourTable(kBanners);
    return table;
}

/// Reads one state out of a colour table.
std::optional<std::uint8_t> colourAt(const std::vector<std::int8_t>& table, const std::int32_t stateId) {
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= table.size()) {
        return std::nullopt;
    }
    const std::int8_t colour = table[static_cast<std::size_t>(stateId)];
    return colour < 0 ? std::nullopt : std::optional{static_cast<std::uint8_t>(colour)};
}

} // namespace

std::optional<std::uint8_t> bedColourOf(const std::int32_t stateId) { return colourAt(bedColours(), stateId); }

std::optional<std::uint8_t> bannerColourOf(const std::int32_t stateId) { return colourAt(bannerColours(), stateId); }

} // namespace basalt::server::runtime::bedrock
