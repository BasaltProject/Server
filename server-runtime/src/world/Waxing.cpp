#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/Waxing.hpp>

#include <string>
#include <string_view>

namespace basalt::server::runtime::world::waxing {
namespace {

constexpr std::string_view kWaxed = "waxed_";
constexpr std::string_view kStripped = "stripped_";

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

/// The named block wearing this state's properties. parity: `Block.withPropertiesOf`, which carries
[[nodiscard]] std::optional<std::int32_t> sameShape(std::int32_t stateId, const std::string& path) {
    const registry::BlockState* plain = registry::blocks::named(path);
    if (plain == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(path), registry::blockStateDefinitions::propertiesOf(stateId), std::nullopt);
    return turned.has_value() ? turned : std::optional{plain->stateId};
}

} // namespace

std::optional<std::int32_t> waxed(const std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    if (path.empty() || path.starts_with(kWaxed)) {
        return std::nullopt;
    }
    return sameShape(stateId, std::string{kWaxed} + path);
}

std::optional<Scrape> scraped(const std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    if (path.empty()) {
        return std::nullopt;
    }
    const auto asScrape = [](const std::optional<std::int32_t>& turned, Cut cut) -> std::optional<Scrape> {
        return turned.has_value() ? std::optional{Scrape{.stateId = *turned, .cut = cut}} : std::nullopt;
    };
    // parity: the strip comes first, and a stripped log is not strippable again.
    if (!path.starts_with(kStripped)) {
        const std::optional<Scrape> stripped =
            asScrape(sameShape(stateId, std::string{kStripped} + path), Cut::Stripped);
        if (stripped.has_value()) {
            return stripped;
        }
    }
    // parity: `WeatheringCopper.getPrevious`.
    const std::optional<std::string_view> fresher = registry::weathering::previous(path);
    if (fresher.has_value()) {
        return asScrape(sameShape(stateId, std::string{*fresher}), Cut::Scraped);
    }
    return path.starts_with(kWaxed) ? asScrape(sameShape(stateId, path.substr(kWaxed.size())), Cut::Unwaxed)
                                    : std::nullopt;
}

} // namespace basalt::server::runtime::world::waxing
