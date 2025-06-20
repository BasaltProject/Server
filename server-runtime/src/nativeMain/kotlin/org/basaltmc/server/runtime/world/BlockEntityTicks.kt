package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.loot.ItemStack

/**
 * The block entities that do something on their own, ticked once each per tick.
 *
 * parity: `LevelChunk.tickersInLevel`, the list vanilla keeps per chunk so that a tick visits the
 * few block entities that want one instead of every chest in the world. Here the chunk is asked for
 * its block entities and the ones with a ticker are picked out by type, which is the same walk over
 * a much shorter list.
 *
 * Only the chunks the caller passes are visited, so a furnace far from every player is as still as
 * vanilla's.
 */
public object BlockEntityTicks {
    /** A block entity that changed, so the caller can tell whoever is watching that chunk. */
    public class Changed(
        public val x: Int,
        public val y: Int,
        public val z: Int,
        public val stateId: Int,
    )

    /**
     * Ticks every block entity in [chunks] that has something to do.
     *
     * [recipeFor] answers what a given cooker makes of a stack; the world's recipe book is the one
     * that knows, and this has no business reaching for it.
     */
    public fun tick(
        world: LoadedWorld,
        chunks: List<IntArray>,
        recipeFor: (Recipe.Cooker, ItemStack) -> Recipe.Cooking?,
    ): List<Changed> {
        val changes = mutableListOf<Changed>()
        // parity: `DaylightDetectorBlock`'s own ticker, which looks once every twenty ticks.
        if (world.gameTime % DaylightDetectors.INTERVAL_TICKS == 0L) {
            for (position in chunks) {
                val chunk = world.state.chunkIfLoaded(position[0], position[1]) ?: continue
                for (entity in chunk.blockEntities()) {
                    if (entity.type.path != "daylight_detector") continue
                    val x = (position[0] shl 4) + entity.localX
                    val z = (position[1] shl 4) + entity.localZ
                    DaylightDetectors.update(world, x, entity.y, z)?.let {
                        changes += Changed(x, entity.y, z, it.stateId)
                    }
                }
            }
        }
        for (position in chunks) {
            val chunk = world.state.chunkIfLoaded(position[0], position[1]) ?: continue
            if (chunk.blockEntityCount == 0) continue
            for (entity in chunk.blockEntities()) {
                val x0 = (position[0] shl 4) + entity.localX
                val z0 = (position[1] shl 4) + entity.localZ
                // parity: `HopperBlockEntity.pushItemsTick`, one move per turn and then a rest.
                if (entity.type.path == "hopper") {
                    if (Hoppers.tick(world, x0, entity.y, z0)) chunk.markDirty()
                    continue
                }
                // parity: `BrewingStandBlockEntity.serverTick`, which also shows what it holds on
                // the block itself.
                if (entity.type.path == "brewing_stand") {
                    val stand = BrewingStands.read(entity.data)
                    if (BrewingStands.tick(stand)) {
                        BrewingStands.write(stand, entity.data)
                        chunk.markDirty()
                        bottleChange(world, x0, entity.y, z0, BrewingStands.bottleBits(stand))
                            ?.let { changes += Changed(x0, entity.y, z0, it) }
                    }
                    continue
                }
                val cooker = Furnaces.COOKERS[entity.type.path] ?: continue
                val x = (position[0] shl 4) + entity.localX
                val z = (position[1] shl 4) + entity.localZ
                val state = Furnaces.read(entity.data)
                if (!Furnaces.tick(state, { stack -> recipeFor(cooker, stack) })) continue
                Furnaces.write(state, entity.data)
                chunk.markDirty()
                litChange(world, x, entity.y, z, state.lit)?.let { changes += Changed(x, entity.y, z, it) }
            }
        }
        return changes
    }

    /** parity: the three `HAS_BOTTLE` bits a stand shows for the bottles standing in it. */
    private fun bottleChange(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        bottles: BooleanArray,
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val wanted = mutableMapOf<String, String>()
        for (slot in bottles.indices) wanted["has_bottle_$slot"] = bottles[slot].toString()
        val turned = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        if (turned == stateId) return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return turned
    }

    /**
     * parity: the `state.setValue(LIT, isLit)` at the end of `serverTick`: the block itself carries
     * whether the fire is burning, which is what the client draws and what light comes off it.
     */
    private fun litChange(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        lit: Boolean,
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (BlockStateDefinitions.propertiesOf(stateId)["lit"] == lit.toString()) return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("lit" to lit.toString()), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return turned
    }
}
