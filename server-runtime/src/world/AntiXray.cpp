#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/AntiXray.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <random>

namespace basalt::server::runtime::world {
namespace {

constexpr std::int32_t kSectionSize = 16;
constexpr std::size_t kCells = 16 * 16 * 16;
constexpr std::array<std::array<std::int32_t, 3>, 6> kSteps{
    {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}}};

struct XorShift {
    std::uint32_t state;

    [[nodiscard]] static XorShift seeded() {
        static thread_local std::mt19937 source{std::random_device{}()};
        std::uint32_t seed = 0;
        while (seed == 0) {
            seed = static_cast<std::uint32_t>(source());
        }
        return XorShift{seed};
    }

    [[nodiscard]] std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    [[nodiscard]] std::size_t below(const std::size_t bound) {
        return static_cast<std::size_t>((static_cast<std::uint64_t>(next()) * bound) >> 32);
    }
};

[[nodiscard]] std::size_t cell(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    return static_cast<std::size_t>((y << 8) | (z << 4) | x);
}

void markBlock(std::vector<bool>& table, const std::string& name) {
    const wire::common::Identifier id = wire::common::Identifier::vanilla(name);
    const registry::blockStateDefinitions::Definition* definition = registry::blockStateDefinitions::of(id);
    if (definition == nullptr || registry::blocks::isAir(definition->firstStateId())) {
        return;
    }
    for (std::int32_t i = 0; i < definition->stateCount(); i++) {
        table[static_cast<std::size_t>(definition->firstStateId() + i)] = true;
    }
}

[[nodiscard]] bool isSolidBlock(const std::int32_t stateId, const bool lavaObscures) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    if (lavaObscures && path == "lava") {
        return true;
    }
    if (path == "spawner" || path == "barrier" || path == "slime_block" || path == "mangrove_roots" ||
        path.ends_with("shulker_box")) {
        return false;
    }
    return registry::blockSupport::conductsRedstone(stateId);
}

std::vector<std::array<std::int32_t, 3>> offsetsWithin(const std::int32_t radius) {
    std::vector<std::array<std::int32_t, 3>> offsets;
    for (std::int32_t distance = 1; distance <= radius; distance++) {
        for (std::int32_t dx = -distance; dx <= distance; dx++) {
            for (std::int32_t dy = -distance; dy <= distance; dy++) {
                for (std::int32_t dz = -distance; dz <= distance; dz++) {
                    if (std::abs(dx) + std::abs(dy) + std::abs(dz) == distance) {
                        offsets.push_back({dx, dy, dz});
                    }
                }
            }
        }
    }
    return offsets;
}

} // namespace

AntiXray::AntiXray(const config::AntiXrayRules& rules, const config::WorldDimension dimension)
    : mode_(rules.engineMode), limit_((rules.maxBlockHeight >> 4) << 4), radius_(rules.updateRadius),
      usePermission_(rules.usePermission), dimension_(dimension), offsets_(offsetsWithin(rules.updateRadius)) {
    const auto states = static_cast<std::size_t>(registry::blocks::stateCount());
    solid_.assign(states, false);
    hidden_.assign(states, false);
    eligible_.assign(states, false);
    for (std::size_t id = 0; id < states; id++) {
        solid_[id] = isSolidBlock(static_cast<std::int32_t>(id), rules.lavaObscures);
    }
    for (const std::string& name : rules.hiddenBlocks) {
        markBlock(hidden_, name);
    }
    eligible_ = hidden_;
    if (mode_ != 1) {
        for (const std::string& name : rules.replacementBlocks) {
            markBlock(eligible_, name);
        }
        for (const std::string& name : rules.hiddenBlocks) {
            const std::optional<std::int32_t> state =
                registry::blocks::defaultStateIdOf(wire::common::Identifier::vanilla(name));
            if (state.has_value() && !registry::blocks::isAir(*state) &&
                !registry::blockConnections::hasBlockEntity(*state)) {
                fakes_.push_back(*state);
            }
        }
        if (fakes_.empty()) {
            fakes_.push_back(
                registry::blocks::defaultStateIdOf(wire::common::Identifier::vanilla("diamond_ore")).value_or(0));
        }
    }
}

bool AntiXray::opens(const std::int32_t before, const std::int32_t after) const noexcept {
    return solid(before) && !solid(after);
}

std::vector<std::vector<std::int32_t>> AntiXray::obfuscate(const server::world::Chunk& chunk,
                                                           const ChunkLookup& neighbours) const {
    const std::int32_t minY = chunk.verticalBounds().minY();
    const std::size_t sections = chunk.sectionCount();
    std::vector<std::vector<std::int32_t>> out(sections);
    const server::world::Chunk* around[4] = {
        neighbours(chunk.chunkX() - 1, chunk.chunkZ()), neighbours(chunk.chunkX() + 1, chunk.chunkZ()),
        neighbours(chunk.chunkX(), chunk.chunkZ() - 1), neighbours(chunk.chunkX(), chunk.chunkZ() + 1)};
    XorShift random = XorShift::seeded();
    std::vector<std::int32_t> below(kCells);
    std::vector<std::int32_t> above(kCells);
    std::vector<std::int32_t> own(kCells);

    const auto transparent = [this](const std::int32_t stateId) { return !solid(stateId); };
    for (std::size_t index = 0; index < sections; index++) {
        const std::int32_t baseY = minY + static_cast<std::int32_t>(index) * kSectionSize;
        if (baseY >= limit_) {
            break;
        }
        chunk.section(index).copyBlockStateIdsInto(own);
        if (std::none_of(own.begin(), own.end(),
                         [this](const std::int32_t id) { return eligible_[static_cast<std::size_t>(id)]; })) {
            continue;
        }
        const bool hasBelow = index > 0;
        const bool hasAbove = index + 1 < sections;
        if (hasBelow) {
            chunk.section(index - 1).copyBlockStateIdsInto(below);
        }
        if (hasAbove) {
            chunk.section(index + 1).copyBlockStateIdsInto(above);
        }
        std::int32_t base = 0;
        if (mode_ == 1) {
            const char* name = dimension_ == config::WorldDimension::Nether ? "netherrack"
                               : dimension_ == config::WorldDimension::End  ? "end_stone"
                               : baseY < 0                                  ? "deepslate"
                                                                            : "stone";
            base = registry::blocks::defaultStateIdOf(wire::common::Identifier::vanilla(name)).value_or(0);
        }
        std::vector<std::int32_t> replaced = own;
        bool touched = false;
        for (std::int32_t y = 0; y < kSectionSize; y++) {
            const std::int32_t layerFake = mode_ == 3 ? fakes_[random.below(fakes_.size())] : 0;
            for (std::int32_t z = 0; z < kSectionSize; z++) {
                for (std::int32_t x = 0; x < kSectionSize; x++) {
                    const std::int32_t id = own[cell(x, y, z)];
                    if (!eligible_[static_cast<std::size_t>(id)]) {
                        continue;
                    }
                    const auto at = [&](const std::int32_t dx, const std::int32_t dy, const std::int32_t dz) {
                        const std::int32_t nx = x + dx;
                        const std::int32_t ny = y + dy;
                        const std::int32_t nz = z + dz;
                        if (ny < 0) {
                            return hasBelow ? below[cell(nx, kSectionSize - 1, nz)] : -1;
                        }
                        if (ny >= kSectionSize) {
                            return hasAbove ? above[cell(nx, 0, nz)] : -1;
                        }
                        if (nx >= 0 && nx < kSectionSize && nz >= 0 && nz < kSectionSize) {
                            return own[cell(nx, ny, nz)];
                        }
                        const server::world::Chunk* side = nx < 0               ? around[0]
                                                           : nx >= kSectionSize ? around[1]
                                                           : nz < 0             ? around[2]
                                                                                : around[3];
                        return side == nullptr ? -1 : side->blockStateId(nx & 15, baseY + ny, nz & 15);
                    };
                    bool exposed = false;
                    for (const std::array<std::int32_t, 3>& step : kSteps) {
                        const std::int32_t neighbour = at(step[0], step[1], step[2]);
                        if (neighbour < 0 || transparent(neighbour)) {
                            exposed = true;
                            break;
                        }
                    }
                    if (exposed) {
                        continue;
                    }
                    replaced[cell(x, y, z)] = mode_ == 1   ? base
                                              : mode_ == 3 ? layerFake
                                                           : fakes_[random.below(fakes_.size())];
                    touched = true;
                }
            }
        }
        if (touched) {
            out[index] = std::move(replaced);
        }
    }
    return out;
}

} // namespace basalt::server::runtime::world
