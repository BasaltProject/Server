#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/bedrock/BedrockBlockEntities.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

/// The dyes in the order every edition numbers them, which is what a bed's colour byte counts in.
constexpr std::array<std::string_view, 16> kDyes{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                 "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                 "brown", "green",  "red",        "black"};

/// Every bed state's colour, by state id, built once. The world hands out state ids by the million
/// and asking the registry for each one's name is not something a chunk can afford.
const std::vector<std::int8_t>& bedColours() {
    static const std::vector<std::int8_t> table = [] {
        std::vector<std::int8_t> out(static_cast<std::size_t>(server::registry::blocks::stateCount()), -1);
        for (std::size_t colour = 0; colour < kDyes.size(); ++colour) {
            const wire::common::Identifier bed = wire::common::Identifier::vanilla(std::string{kDyes[colour]} + "_bed");
            const auto first = server::registry::blocks::defaultStateIdOf(bed);
            if (!first) {
                continue;
            }
            // One block's states are laid out together, so the run is found by asking each one who
            // it belongs to until the answer changes.
            for (auto state = *first; state < static_cast<std::int32_t>(out.size()); ++state) {
                const auto what = server::registry::blocks::byStateId(state);
                if (!what || what->block != bed) {
                    break;
                }
                out[static_cast<std::size_t>(state)] = static_cast<std::int8_t>(colour);
            }
            for (auto state = *first - 1; state >= 0; --state) {
                const auto what = server::registry::blocks::byStateId(state);
                if (!what || what->block != bed) {
                    break;
                }
                out[static_cast<std::size_t>(state)] = static_cast<std::int8_t>(colour);
            }
        }
        return out;
    }();
    return table;
}

} // namespace

std::optional<std::uint8_t> bedColourOf(const std::int32_t stateId) {
    const std::vector<std::int8_t>& table = bedColours();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= table.size()) {
        return std::nullopt;
    }
    const std::int8_t colour = table[static_cast<std::size_t>(stateId)];
    return colour < 0 ? std::nullopt : std::optional{static_cast<std::uint8_t>(colour)};
}

} // namespace basalt::server::runtime::bedrock
