#include <basalt/metamorph/region/ChunkBlob.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/region/ChunkBlob.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <cstring>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace basalt::server::world::region::chunkBlob {
namespace {

namespace blob = metamorph::region::chunkBlob;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr std::size_t kMaxBlockPalette = std::size_t{1} << blob::kBlockMaxIndirectBits;
constexpr std::size_t kMaxBiomePalette = std::size_t{1} << blob::kBiomeMaxIndirectBits;
constexpr std::int32_t kNotResolved = -2;
constexpr std::int32_t kInvalid = -1;

struct TextHash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept {
        return std::hash<std::string_view>{}(text);
    }
};

struct Caches {
    std::unordered_map<std::int32_t, std::string> blockNames;
    std::unordered_map<std::int32_t, std::string> biomeNames;
    std::unordered_map<std::string, std::int32_t, TextHash, std::equal_to<>> blockIds;
    std::unordered_map<std::string, std::int32_t, TextHash, std::equal_to<>> biomeIds;
    std::vector<std::int32_t> blockRefs;
    std::vector<std::int32_t> biomeRefs;
    std::vector<std::uint8_t> payloads;
    std::vector<std::int32_t> palette;
    std::vector<std::uint64_t> words;
    std::vector<std::uint32_t> refs;
};

[[nodiscard]] Caches& caches() {
    thread_local Caches held;
    return held;
}

[[nodiscard]] const std::string& blockName(Caches& cache, std::int32_t stateId) {
    const auto found = cache.blockNames.find(stateId);
    if (found != cache.blockNames.end()) {
        return found->second;
    }
    blob::StateText text;
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    text.id = block == nullptr ? "minecraft:air" : block->toString();
    text.properties = registry::blockStateDefinitions::propertiesOf(stateId);
    return cache.blockNames.emplace(stateId, blob::formatState(text)).first->second;
}

[[nodiscard]] const std::string& biomeName(Caches& cache, std::int32_t biomeId) {
    const auto found = cache.biomeNames.find(biomeId);
    if (found != cache.biomeNames.end()) {
        return found->second;
    }
    const Identifier* name = registry::biomes::registry().nameOf(biomeId);
    return cache.biomeNames.emplace(biomeId, name == nullptr ? "minecraft:plains" : name->toString()).first->second;
}

/// parity: `BlockState.CODEC`, where whatever a palette entry leaves out keeps the block's default.
[[nodiscard]] std::int32_t resolveBlock(Caches& cache, std::string_view text) {
    if (const auto found = cache.blockIds.find(text); found != cache.blockIds.end()) {
        return found->second;
    }
    std::int32_t id = kInvalid;
    if (const std::optional<blob::StateText> state = blob::parseState(text); state.has_value()) {
        if (const auto block = Identifier::parse(state->id); block.has_value()) {
            std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
                *block, state->properties, registry::blocks::defaultStateIdOf(*block));
            if (!stateId.has_value()) {
                stateId = registry::blocks::stateIdOf(*block, state->properties);
            }
            id = stateId.value_or(kInvalid);
        }
    }
    cache.blockIds.emplace(std::string{text}, id);
    return id;
}

[[nodiscard]] std::int32_t resolveBiome(Caches& cache, std::string_view text) {
    if (const auto found = cache.biomeIds.find(text); found != cache.biomeIds.end()) {
        return found->second;
    }
    std::int32_t id = kInvalid;
    if (const auto name = Identifier::parse(text); name.has_value()) {
        id = registry::biomes::registry().idOf(*name).value_or(kInvalid);
    }
    cache.biomeIds.emplace(std::string{text}, id);
    return id;
}

/// Emits one container; false when its palette is wider than the vanilla NBT layouts allow.
template <typename Export, typename NameOf>
[[nodiscard]] bool writeContainer(Caches& cache, blob::ByteWriter& out, std::vector<std::int32_t>& refOf,
                                  std::vector<std::string>& names, std::size_t maxPalette, Export&& exportContainer,
                                  NameOf&& nameOf) {
    int bits = 0;
    exportContainer(cache.palette, bits, cache.words);
    if (cache.palette.size() > maxPalette) {
        return false;
    }
    cache.refs.clear();
    for (const std::int32_t value : cache.palette) {
        std::int32_t& ref = refOf[static_cast<std::size_t>(value)];
        if (ref < 0) {
            ref = static_cast<std::int32_t>(names.size());
            names.push_back(nameOf(value));
        }
        cache.refs.push_back(static_cast<std::uint32_t>(ref));
    }
    blob::writePalette(out, cache.refs, bits, cache.words);
    return true;
}

void forgetRefs(std::vector<std::int32_t>& refOf, std::span<const std::int32_t> used) {
    for (const std::int32_t value : used) {
        refOf[static_cast<std::size_t>(value)] = -1;
    }
}

} // namespace

std::optional<std::vector<std::uint8_t>> write(const Chunk& chunk) {
    Caches& cache = caches();
    if (cache.blockRefs.empty()) {
        cache.blockRefs.assign(static_cast<std::size_t>(registry::blocks::stateCount()), -1);
        cache.biomeRefs.assign(registry::biomes::registry().size(), -1);
    }
    std::vector<std::string> names;
    std::vector<blob::SectionSpan> spans;
    std::vector<std::int32_t> usedBlocks;
    std::vector<std::int32_t> usedBiomes;
    cache.payloads.clear();
    bool fits = true;
    for (std::size_t i = 0; i < chunk.sectionCount() && fits; ++i) {
        const ChunkSection& section = chunk.section(i);
        const std::size_t offset = cache.payloads.size();
        blob::ByteWriter out{cache.payloads};
        out.u8(0);
        fits = writeContainer(
            cache, out, cache.blockRefs, names, kMaxBlockPalette,
            [&](auto& palette, int& bits, auto& words) {
                section.exportBlocks(palette, bits, words);
                usedBlocks.insert(usedBlocks.end(), palette.begin(), palette.end());
            },
            [&](std::int32_t id) { return blockName(cache, id); });
        if (fits) {
            fits = writeContainer(
                cache, out, cache.biomeRefs, names, kMaxBiomePalette,
                [&](auto& palette, int& bits, auto& words) {
                    section.exportBiomes(palette, bits, words);
                    usedBiomes.insert(usedBiomes.end(), palette.begin(), palette.end());
                },
                [&](std::int32_t id) { return biomeName(cache, id); });
        }
        spans.push_back({static_cast<std::int8_t>(chunk.verticalBounds().minSection() + static_cast<std::int32_t>(i)),
                         offset, cache.payloads.size() - offset});
    }
    forgetRefs(cache.blockRefs, usedBlocks);
    forgetRefs(cache.biomeRefs, usedBiomes);
    if (!fits) {
        return std::nullopt;
    }
    const NbtCompound rest = chunkNbt::writeRest(chunk);
    std::vector<std::uint8_t> restBytes;
    if (!rest.empty()) {
        restBytes = wire::nbt::nbtIO::writeAnonymous(NbtTag::ofCompound(rest));
    }
    std::vector<std::uint8_t> bytes;
    blob::assemble(bytes,
                   {chunkNbt::kDataVersion, chunk.chunkX(), chunk.chunkZ(), chunk.verticalBounds().minSection(),
                    chunk.inhabitedTime(), chunkNbt::statusName(chunk.stage())},
                   names, spans, cache.payloads, restBytes);
    return bytes;
}

std::expected<Chunk, ChunkNbtError> read(std::span<const std::uint8_t> bytes, ChunkHeight bounds) {
    const std::optional<blob::View> view = blob::parse(bytes);
    if (!view.has_value()) {
        return std::unexpected(ChunkNbtError{"chunk blob is damaged"});
    }
    if (view->header.dataVersion != chunkNbt::kDataVersion) {
        const std::optional<NbtCompound> nbt = blob::toNbt(bytes);
        if (!nbt.has_value()) {
            return std::unexpected(ChunkNbtError{"chunk blob is damaged"});
        }
        return chunkNbt::read(*nbt, bounds);
    }
    NbtCompound tag;
    if (!view->rest.empty()) {
        const auto rest = wire::nbt::nbtIO::readAnonymous(view->rest);
        const NbtCompound* compound = rest.has_value() ? rest->tag.get<NbtCompound>() : nullptr;
        if (compound == nullptr) {
            return std::unexpected(ChunkNbtError{"chunk blob has a damaged tail"});
        }
        tag = *compound;
    }
    tag.putInt("xPos", view->header.xPos);
    tag.putInt("zPos", view->header.zPos);
    tag.putLong("InhabitedTime", view->header.inhabitedTime);
    tag.putString("Status", std::string{view->header.status});
    Caches& cache = caches();
    return chunkNbt::readCurrent(tag, bounds, [&](Chunk& chunk) -> std::expected<void, ChunkNbtError> {
        std::vector<std::int32_t> blockOf(view->names.size(), kNotResolved);
        std::vector<std::int32_t> biomeOf(view->names.size(), kNotResolved);
        std::vector<std::int32_t> ids;
        const auto resolve = [&](const blob::Palette& palette, std::vector<std::int32_t>& memo, bool block,
                                 std::vector<std::int32_t>& out) -> std::expected<void, ChunkNbtError> {
            out.clear();
            for (const std::uint32_t ref : palette.refs) {
                if (ref >= view->names.size()) {
                    return std::unexpected(ChunkNbtError{"chunk blob names an entry that is not there"});
                }
                std::int32_t& known = memo[ref];
                if (known == kNotResolved) {
                    known = block ? resolveBlock(cache, view->names[ref]) : resolveBiome(cache, view->names[ref]);
                }
                if (known == kInvalid) {
                    return std::unexpected(ChunkNbtError{
                        std::format("unknown {} {}", block ? "block state" : "biome", std::string{view->names[ref]})});
                }
                out.push_back(known);
            }
            return {};
        };
        std::vector<std::int32_t> biomeIds;
        for (const blob::SectionEntry& entry : view->sections) {
            const std::int32_t index = entry.y - bounds.minSection();
            if (index < 0 || static_cast<std::size_t>(index) >= chunk.sectionCount()) {
                continue;
            }
            blob::SectionPayload payload;
            if (!blob::parseSection(entry.payload, payload)) {
                return std::unexpected(ChunkNbtError{"chunk blob has a damaged section"});
            }
            if (const auto blocks = resolve(payload.blocks, blockOf, true, ids); !blocks.has_value()) {
                return blocks;
            }
            if (const auto biomes = resolve(payload.biomes, biomeOf, false, biomeIds); !biomes.has_value()) {
                return biomes;
            }
            if (!chunk.section(static_cast<std::size_t>(index))
                     .restorePacked({ids, payload.blocks.bits, payload.blocks.words},
                                    {biomeIds, payload.biomes.bits, payload.biomes.words})) {
                return std::unexpected(ChunkNbtError{"chunk blob has a damaged section"});
            }
        }
        return {};
    });
}

std::int32_t dataVersionOf(std::span<const std::uint8_t> stored) {
    std::int32_t version = 0;
    if (blob::isBlob(stored) && stored.size() >= 8) {
        std::memcpy(&version, stored.data() + 4, sizeof version);
    }
    return version;
}

} // namespace basalt::server::world::region::chunkBlob
