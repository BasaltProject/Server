#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/player/PlayerBlockContact.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>

namespace basalt::server::runtime::player {
namespace {

constexpr double kHalfWidth = 0.3;
constexpr double kHeight = 1.8;
constexpr double kSneakingHeight = 1.5;
constexpr double kSlack = 1.0e-5;
constexpr double kCactusInset = 1.0 / 16.0;
constexpr std::int32_t kIgniteTicks = 8 * 20;
constexpr float kSoulFireDamage = 2.0F;

} // namespace

void tickBlockContact(PlayerSession& session, const entity::entityCollision::BlockAt& blockAt,
                      random::RandomSource& random,
                      const std::function<void(float, std::string_view, const wire::common::Identifier&)>& hurt,
                      bool magmaHurtsSneaking, float stonecutterDamage) {
    if (session.gamemode == wire::protocol::play::Gamemode::Spectator || !hurt) {
        return;
    }
    namespace mth = wire::common::mth;
    const double height = session.sneaking ? kSneakingHeight : kHeight;
    const double minX = session.x - kHalfWidth + kSlack;
    const double maxX = session.x + kHalfWidth - kSlack;
    const double minZ = session.z - kHalfWidth + kSlack;
    const double maxZ = session.z + kHalfWidth - kSlack;
    const double minY = session.y + kSlack;
    const double maxY = session.y + height - kSlack;
    bool cactus = false;
    float fireDamage = 0.0F;
    for (std::int32_t x = mth::floor(minX); x <= mth::floor(maxX); ++x) {
        for (std::int32_t y = mth::floor(minY); y <= mth::floor(maxY); ++y) {
            for (std::int32_t z = mth::floor(minZ); z <= mth::floor(maxZ); ++z) {
                const std::optional<std::int32_t> state = blockAt(x, y, z);
                const wire::common::Identifier* block =
                    state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
                if (block == nullptr) {
                    continue;
                }
                const std::string_view path = block->path();
                if (path == "cactus") {
                    cactus = cactus || (minX < x + 1 - kCactusInset && maxX > x + kCactusInset &&
                                        minZ < z + 1 - kCactusInset && maxZ > z + kCactusInset);
                } else if (path == "fire" || path == "soul_fire") {
                    fireDamage = std::max(fireDamage, path == "fire" ? 1.0F : kSoulFireDamage);
                }
            }
        }
    }
    if (cactus) {
        hurt(1.0F, "cactus", wire::common::Identifier::vanilla("cactus"));
    }
    if (fireDamage > 0.0F) {
        // parity: `BaseFireBlock.entityInside` clears the freeze, then `fireIgnite` lights the player.
        session.ticksFrozen = 0;
        session.fireTicks += session.fireTicks < 0 ? 1 : random.nextInt(1, 3);
        if (session.fireTicks >= 0) {
            session.fireTicks = std::max(session.fireTicks, kIgniteTicks);
        }
        hurt(fireDamage, "inFire", wire::common::Identifier::vanilla("in_fire"));
    }
    if (session.onGround && (!session.sneaking || magmaHurtsSneaking)) {
        const std::optional<std::int32_t> below =
            blockAt(mth::floor(session.x), mth::floor(session.y - 0.2), mth::floor(session.z));
        const wire::common::Identifier* block =
            below.has_value() ? registry::blockStateDefinitions::blockOf(*below) : nullptr;
        if (block != nullptr && block->path() == "magma_block") {
            hurt(1.0F, "hotFloor", wire::common::Identifier::vanilla("hot_floor"));
        }
        if (block != nullptr && block->path() == "stonecutter" && stonecutterDamage > 0.0F) {
            hurt(stonecutterDamage, "stonecutter", wire::common::Identifier::vanilla("generic"));
        }
    }
}

} // namespace basalt::server::runtime::player
