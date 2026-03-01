#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealRecipes;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// Bone meal on something that grows. Answers whether the click was spent, which is what decides
/// whether the item in hand is then considered as a block to place.
///
/// Shared because both doors hold the same bone meal: the Java session runs it through its own
/// handler and the Bedrock one has no handler at all.
bool useBoneMealFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                    std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

/// A honeycomb waxing copper.
bool useHoneycombFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

/// A flint and steel or a fire charge on a block of tnt: it is taken away and left ticking. The
/// lighter is worn and the charge is spent, which is the only difference between the two.
void primeTntFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                 std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held,
                 bool wears);

/// A flint and steel on the square beside what was clicked: a fire, or the portal that frame makes.
void igniteFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
               std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

/// An axe stripping a log, scraping oxidation off copper or taking the wax back off it.
bool useAxeFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
               std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

/// A composter: bone meal out of a ready one, or one item into one with room. Answers whether the
/// click was spent, which for a composter is true even when the item did not raise the level.
bool useComposterFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held);

/// One piece of food onto a lit campfire, if that campfire has a free corner and a recipe for it.
bool placeOnCampfireFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                        std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                        const std::optional<storage::StoredItemStack>& held, world::RealRecipes* recipes);

} // namespace basalt::server::runtime::player
