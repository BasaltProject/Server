#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSigns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <optional>
#include <utility>

namespace basalt::server::runtime::player {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

/// parity: `SignBlockEntity`'s own four.
constexpr std::size_t kSignLines = 4;

} // namespace

std::vector<std::string> signLinesOf(const std::string_view text) {
    std::vector<std::string> lines;
    std::size_t at = 0;
    while (at <= text.size() && lines.size() < kSignLines) {
        const std::size_t stop = text.find('\n', at);
        lines.emplace_back(text.substr(at, stop == std::string_view::npos ? std::string_view::npos : stop - at));
        if (stop == std::string_view::npos) {
            break;
        }
        at = stop + 1;
    }
    lines.resize(kSignLines);
    return lines;
}

bool writeSignFor(const BreakContext& context, world::LoadedWorld& world, PlayerSession& session, const std::int32_t x,
                  const std::int32_t y, const std::int32_t z, const bool front, std::vector<std::string> lines,
                  const SignChangeAllowed& allowed) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const std::optional<Identifier> type = stateId.has_value() ? world::signs::typeOf(*stateId) : std::nullopt;
    server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (!type.has_value() || chunk == nullptr) {
        return false;
    }
    server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15);
    // parity: `SignBlockEntity.isWaxed`, which is what stops a signed sign being rewritten.
    if (existing != nullptr && world::signs::isWaxed(existing->data())) {
        return false;
    }
    if (allowed && !allowed(session, x, y, z, lines)) {
        return false;
    }
    const wire::nbt::NbtCompound base = existing == nullptr ? world::signs::emptyData() : existing->data();
    const wire::nbt::NbtCompound data = world.state().gameplay().blocks.signAllowColors
                                            ? world::signs::withColouredText(base, front, lines)
                                            : world::signs::withText(base, front, lines);
    chunk->setBlockEntity(world::signs::blockEntity(x & 15, y, z & 15, *type, data));
    const std::optional<std::int32_t> typeId = reg::blockEntityTypes::idOf(*type);
    if (!typeId.has_value()) {
        return true;
    }
    const wire::protocol::play::BlockEntityDataPacket packet{
        .pos = {.x = x, .y = y, .z = z}, .typeId = *typeId, .tag = data};
    for (const PlayerSession* player : playersIn(context, world.name())) {
        if (player->connection().sendSignText) {
            player->connection().sendSignText(x, y, z, world::signs::plainText(data, true),
                                              world::signs::plainText(data, false), world::signs::isWaxed(data));
            continue;
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::BlockEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    return true;
}

} // namespace basalt::server::runtime::player
