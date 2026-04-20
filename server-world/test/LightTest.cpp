#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkDataCodec.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>

#include <doctest.h>

#include <cstdint>

using namespace basalt::server::world;
namespace registry = basalt::server::registry;

TEST_CASE("the encoded sections carry both counts and both containers") {
    // parity: LevelChunkSection.write always writes two shorts, never just one.
    Chunk chunk{0, 0};
    chunk.setBlockStateId(0, 0, 0, registry::blocks::stone().stateId);
    chunk.setBlockStateId(1, 0, 0, registry::blocks::water().stateId);

    const std::vector<std::uint8_t> encoded = chunkDataCodec::encodeSections(chunk);
    basalt::wire::io::ByteBuffer buffer = basalt::wire::io::ByteBuffer::wrap(encoded);
    const auto sectionIndex = static_cast<std::size_t>((0 >> 4) - chunk.verticalBounds().minSection());
    for (std::size_t skipped = 0; skipped < sectionIndex; ++skipped) {
        CHECK(buffer.readShort() == 0);
        CHECK(buffer.readShort() == 0);
        static_cast<void>(basalt::wire::protocol::play::palettedContainer::read(buffer, 4096, 8));
        static_cast<void>(basalt::wire::protocol::play::palettedContainer::read(buffer, 64, 3));
    }
    CHECK(buffer.readShort() == 2);
    CHECK(buffer.readShort() == 1);
    const std::vector<std::int32_t> blocks = basalt::wire::protocol::play::palettedContainer::read(buffer, 4096, 8);
    CHECK(blocks[0] == registry::blocks::stone().stateId);
    CHECK(blocks[1] == registry::blocks::water().stateId);
    CHECK_FALSE(buffer.failed());
}

TEST_CASE("the heightmap is packed into the storage the client expects") {
    Chunk chunk{0, 0};
    chunk.setBlockStateId(0, 64, 0, registry::blocks::stone().stateId);
    const std::vector<std::int64_t> packed = chunkDataCodec::encodeHeightmap(chunk);
    CHECK(packed.size() == 37);

    basalt::wire::protocol::play::BitStorage storage{9, 256, std::vector<std::uint64_t>{packed.begin(), packed.end()}};
    CHECK(storage.get(0) == 65 - chunk.verticalBounds().minY());
}

TEST_CASE("an unlit chunk sends empty masks rather than empty arrays") {
    Chunk chunk{0, 0};
    const chunkDataCodec::LightData data = chunkDataCodec::encodeLight(chunk, false);
    CHECK(data.skyLight.empty());
    CHECK(data.blockLight.empty());
    CHECK(data.skyLightMask.front() == 0);

    const chunkDataCodec::LightData lit = chunkDataCodec::encodeLight(chunk, true);
    CHECK(lit.skyLight.size() == chunk.sectionCount());
    CHECK(lit.emptySkyLightMask.front() == 0);
}
