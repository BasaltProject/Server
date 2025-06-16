package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.random.Random

/** The two fluids that flow, and the sixteen block states each of them owns. */
public enum class FluidKind(
    public val id: Identifier,
    public val baseStateId: Int,
) {
    WATER(Identifier.vanilla("water"), Blocks.WATER.stateId),
    LAVA(Identifier.vanilla("lava"), Blocks.LAVA.stateId),
}

/**
 * Water and lava spreading, one scheduled tick at a time.
 *
 * parity: `FlowingFluid`, with `WaterFluid`/`LavaFluid` for the constants and `LiquidBlock` for
 * when a tick is scheduled. A fluid moves only when [onBlockChanged] wakes it.
 *
 * A change is `x, y, z, new state, washed-away state`: the last is what the caller drops loot for,
 * which is why the list is not just positions.
 */
public object FluidFlow {
    /** parity: `WaterFluid.getTickDelay`, and `LavaFluid`'s, which is faster in the Nether. */
    public const val WATER_TICK_DELAY: Int = 5
    public const val LAVA_TICK_DELAY: Int = 30
    public const val FAST_LAVA_TICK_DELAY: Int = 10

    private const val EMPTY = -1
    private const val MAX_AMOUNT = 8
    private const val FALLING_LEVEL = 8

    /** parity: `FlowingFluid.getSlopeDistance`'s recursion, capped by `getSlopeFindDistance`. */
    private const val WATER_SLOPE_DISTANCE = 4
    private const val FAST_LAVA_SLOPE_DISTANCE = 4
    private const val LAVA_SLOPE_DISTANCE = 2

    private val OFFSETS =
        arrayOf(
            intArrayOf(0, -1, 0),
            intArrayOf(0, 1, 0),
            intArrayOf(-1, 0, 0),
            intArrayOf(1, 0, 0),
            intArrayOf(0, 0, -1),
            intArrayOf(0, 0, 1),
        )
    private const val DOWN = 0
    private const val UP = 1
    private val HORIZONTAL = 2..5

    /** The face of [OFFSETS], in the same order, for asking a block shape about a wall. */
    private val FACES =
        arrayOf(BlockFace.DOWN, BlockFace.UP, BlockFace.WEST, BlockFace.EAST, BlockFace.NORTH, BlockFace.SOUTH)

    private fun state(name: String): Int = Blocks.REGISTRY.idOf(Identifier.vanilla(name)) ?: Blocks.AIR.stateId

    private val OBSIDIAN = Blocks.OBSIDIAN.stateId
    private val COBBLESTONE = state("cobblestone")
    private val BASALT = state("basalt")
    private val SOUL_SOIL = state("soul_soil")
    private val BLUE_ICE = Blocks.BLUE_ICE.stateId

    /** parity: `FlowingFluid.canHoldAnyFluid`'s own exclusion list, the blocks a flow refuses to enter. */
    private val NEVER_FLOODED: Set<Int> =
        buildSet {
            for (name in listOf(
                "ladder",
                "sugar_cane",
                "bubble_column",
                "nether_portal",
                "end_portal",
                "end_gateway",
                "structure_void",
            )) {
                Blocks.REGISTRY.idOf(Identifier.vanilla(name))?.let(::add)
            }
        }

    public fun kindOf(stateId: Int): FluidKind? =
        when {
            stateId in FluidKind.LAVA.baseStateId..FluidKind.LAVA.baseStateId + 15 -> FluidKind.LAVA
            Blocks.hasWaterFluid(stateId) -> FluidKind.WATER
            else -> null
        }

    /**
     * Runs the fluid at this position, returning every block it changed as `(x, y, z, state)`.
     *
     * parity: `FlowingFluid.tick`: a flowing block first settles to whatever its neighbours now
     * justify (which is how a cut-off flow drains), and then spreads from there. A source only
     * spreads.
     */
    public fun tick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray> {
        val changes = mutableListOf<IntArray>()
        Flow(world, changes).tick(x, y, z)
        return changes
    }

    /**
     * Wakes the fluids around a block that just changed, returning whatever changed on the spot.
     *
     * parity: `LiquidBlock.onPlace`/`neighborChanged`/`updateShape`, which all end in the same
     * `scheduleTick`. Lava is the one that does not wait for its tick: water arriving next to it
     * turns it to rock there and then, which is why water reaches a lava flow and never mixes.
     */
    public fun onBlockChanged(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray> {
        val changes = mutableListOf<IntArray>()
        Flow(world, changes).blockChanged(x, y, z)
        return changes
    }

    private fun tickDelay(
        world: LoadedWorld,
        kind: FluidKind,
    ): Int =
        when {
            kind == FluidKind.WATER -> WATER_TICK_DELAY
            isFastLava(world) -> FAST_LAVA_TICK_DELAY
            else -> LAVA_TICK_DELAY
        }

    /** parity: `LavaFluid.isFastLava`, the `ultrawarm` dimension type, which is the Nether's. */
    private fun isFastLava(world: LoadedWorld): Boolean = world.declaration.dimension.hasCeiling

    private fun slopeFindDistance(
        world: LoadedWorld,
        kind: FluidKind,
    ): Int =
        when {
            kind == FluidKind.WATER -> WATER_SLOPE_DISTANCE
            isFastLava(world) -> FAST_LAVA_SLOPE_DISTANCE
            else -> LAVA_SLOPE_DISTANCE
        }

    /** parity: `getDropOff`: water loses one level per block, overworld lava two. */
    private fun dropOff(
        world: LoadedWorld,
        kind: FluidKind,
    ): Int = if (kind == FluidKind.WATER || isFastLava(world)) 1 else 2

    /** parity: `FlowingFluid.getLegacyLevel` read backwards: block level to fluid amount. */
    private fun amountOf(stateId: Int): Int {
        val kind = kindOf(stateId) ?: return 0
        val level = levelOf(stateId, kind)
        return when {
            level == 0 -> MAX_AMOUNT
            level < FALLING_LEVEL -> MAX_AMOUNT - level
            else -> 2 * MAX_AMOUNT - level
        }
    }

    private fun levelOf(
        stateId: Int,
        kind: FluidKind,
    ): Int = if (stateId in kind.baseStateId..kind.baseStateId + 15) stateId - kind.baseStateId else 0

    private fun isFalling(stateId: Int): Boolean {
        val kind = kindOf(stateId) ?: return false
        return levelOf(stateId, kind) >= FALLING_LEVEL
    }

    private fun isSource(stateId: Int): Boolean {
        val kind = kindOf(stateId) ?: return false
        return levelOf(stateId, kind) == 0
    }

    /** parity: `FlowingFluid.getLegacyLevel`: the block state an amount plus falling flag maps to. */
    private fun stateFor(
        kind: FluidKind,
        amount: Int,
        falling: Boolean,
    ): Int {
        if (amount >= MAX_AMOUNT && !falling) return kind.baseStateId
        val level = MAX_AMOUNT - minOf(amount, MAX_AMOUNT) + if (falling) FALLING_LEVEL else 0
        return kind.baseStateId + level
    }

    /** parity: `BlockState.blocksMotion`, read from the block's own shape rather than guessed. */
    private fun blocksMotion(stateId: Int): Boolean =
        !Blocks.isAir(stateId) && !isLiquidBlock(stateId) && BlockShapes.blocksMotion(stateId)

    private fun isLiquidBlock(stateId: Int): Boolean =
        stateId in FluidKind.WATER.baseStateId..FluidKind.WATER.baseStateId + 15 ||
            stateId in FluidKind.LAVA.baseStateId..FluidKind.LAVA.baseStateId + 15

    private fun canHoldAnyFluid(stateId: Int): Boolean = !blocksMotion(stateId) && stateId !in NEVER_FLOODED

    /**
     * One run of the engine, so the world and the change list are not passed through twenty calls.
     *
     * An unloaded neighbour reads as bedrock rather than being generated: vanilla would load it,
     * and generating a chunk from the tick thread is the one thing this loop cannot afford.
     */
    private class Flow(
        val world: LoadedWorld,
        val changes: MutableList<IntArray>,
    ) {
        /**
         * Blocks read during this run, kept so the slope search does not ask the world for the
         * same position twenty times. parity: `FlowingFluid.SpreadContext` caches for the same
         * reason; here every miss is also a lock on the world.
         */
        private val states = HashMap<Long, Int>()

        fun at(
            x: Int,
            y: Int,
            z: Int,
        ): Int =
            states.getOrPut(packed(x, y, z)) {
                world.state.getBlockState(x, y, z) ?: Blocks.BEDROCK.stateId
            }

        /**
         * Writing the same state again is not a change, and treating it as one is what turns a
         * settled ocean into a tick that reschedules itself for ever. parity: `LevelChunk`
         * returns early on an identical state, so vanilla never fires the updates either.
         */
        fun set(
            x: Int,
            y: Int,
            z: Int,
            stateId: Int,
        ) {
            val previous = at(x, y, z)
            if (previous == stateId) return
            if (!world.state.setBlockState(x, y, z, stateId)) return
            states[packed(x, y, z)] = stateId
            // The fifth number is whatever the flow washed away, so the caller can drop it.
            // parity: `FlowingFluid.spreadTo` calls `beforeDestroyingBlock` before the block goes.
            changes += intArrayOf(x, y, z, stateId, previous)
            blockChanged(x, y, z)
        }

        /** parity: `LiquidBlock.neighborChanged` on all six sides of what just changed. */
        fun blockChanged(
            x: Int,
            y: Int,
            z: Int,
        ) {
            wake(x, y, z)
            for (offset in OFFSETS) wake(x + offset[0], y + offset[1], z + offset[2])
        }

        fun wake(
            x: Int,
            y: Int,
            z: Int,
        ) {
            val state = at(x, y, z)
            val kind = kindOf(state) ?: return
            if (solidify(x, y, z, state, kind)) return
            world.state.scheduleFluidTick(x, y, z, kind.id, tickDelay(world, kind))
        }

        fun tick(
            x: Int,
            y: Int,
            z: Int,
        ) {
            var state = at(x, y, z)
            val kind = kindOf(state) ?: return
            if (solidify(x, y, z, state, kind)) return

            if (!isSource(state)) {
                val settled = newLiquid(x, y, z, kind)
                if (settled == EMPTY) {
                    set(x, y, z, Blocks.AIR.stateId)
                    return
                }
                if (settled != state) {
                    state = settled
                    set(x, y, z, settled)
                    world.state.scheduleFluidTick(x, y, z, kind.id, spreadDelay(kind))
                }
            }
            spread(x, y, z, state, kind)
        }

        /** parity: `LavaFluid.getSpreadDelay`: lava rising into a block usually takes four times as long. */
        fun spreadDelay(kind: FluidKind): Int {
            val base = tickDelay(world, kind)
            // Not a parity gap despite the look of it: vanilla rolls this on `Level.random`, which
            // is a `RandomSource.create()` with no seed of its own, so the same lava does not
            // reproduce its own timing there either.
            return if (kind == FluidKind.LAVA && Random.nextInt(4) != 0) base * 4 else base
        }

        /** parity: `LiquidBlock.shouldSpreadLiquid`: lava that touches water turns to rock instead of flowing. */
        fun solidify(
            x: Int,
            y: Int,
            z: Int,
            state: Int,
            kind: FluidKind,
        ): Boolean {
            if (kind != FluidKind.LAVA) return false
            val overSoulSoil = at(x, y - 1, z) == SOUL_SOIL
            // The five directions lava can flow, looked at from the other side: water above or
            // beside it stops it, water below does not.
            for (index in intArrayOf(UP, 2, 3, 4, 5)) {
                val offset = OFFSETS[index]
                val neighbour = at(x + offset[0], y + offset[1], z + offset[2])
                if (kindOf(neighbour) == FluidKind.WATER) {
                    set(x, y, z, if (isSource(state)) OBSIDIAN else COBBLESTONE)
                    return true
                }
                if (overSoulSoil && neighbour == BLUE_ICE) {
                    set(x, y, z, BASALT)
                    return true
                }
            }
            return false
        }

        /** parity: `FlowingFluid.getNewLiquid`, the amount a position is entitled to from its neighbours. */
        fun newLiquid(
            x: Int,
            y: Int,
            z: Int,
            kind: FluidKind,
        ): Int {
            var highest = 0
            var sources = 0
            val here = at(x, y, z)
            for (index in HORIZONTAL) {
                val offset = OFFSETS[index]
                val neighbour = at(x + offset[0], y + offset[1], z + offset[2])
                if (kindOf(neighbour) != kind) continue
                if (!canPassThroughWall(here, neighbour, FACES[index])) continue
                if (isSource(neighbour)) sources++
                highest = maxOf(highest, amountOf(neighbour))
            }

            if (sources >= 2 && canConvertToSource(kind)) {
                val below = at(x, y - 1, z)
                if (blocksMotion(below) || (kindOf(below) == kind && isSource(below))) {
                    return stateFor(kind, MAX_AMOUNT, falling = false)
                }
            }

            val above = at(x, y + 1, z)
            if (kindOf(above) == kind && canPassThroughWall(here, above, BlockFace.UP)) {
                return stateFor(kind, MAX_AMOUNT, falling = true)
            }

            val amount = highest - dropOff(world, kind)
            return if (amount <= 0) EMPTY else stateFor(kind, amount, falling = false)
        }

        /** parity: the `water_source_conversion`/`lava_source_conversion` rules infinite water rides on. */
        fun canConvertToSource(kind: FluidKind): Boolean =
            if (kind == FluidKind.WATER) {
                world.gameRules[GameRuleRegistry.WATER_SOURCE_CONVERSION]
            } else {
                world.gameRules[GameRuleRegistry.LAVA_SOURCE_CONVERSION]
            }

        /** parity: `FlowingFluid.spread`: down first, and sideways only when down is not an option. */
        fun spread(
            x: Int,
            y: Int,
            z: Int,
            state: Int,
            kind: FluidKind,
        ) {
            val below = at(x, y - 1, z)
            if (canMaybePassThrough(kind, state, below, BlockFace.DOWN)) {
                val newBelow = newLiquid(x, y - 1, z, kind)
                if (newBelow != EMPTY && canBeReplacedWith(below, kind, downward = true)) {
                    spreadTo(x, y - 1, z, kind, downward = true, target = newBelow)
                    if (sourceNeighbourCount(x, y, z, kind) >= 3) spreadToSides(x, y, z, state, kind)
                    return
                }
            }
            if (isSource(state) || !isHole(x, y, z, kind)) spreadToSides(x, y, z, state, kind)
        }

        fun spreadToSides(
            x: Int,
            y: Int,
            z: Int,
            state: Int,
            kind: FluidKind,
        ) {
            val amount = if (isFalling(state)) MAX_AMOUNT - 1 else amountOf(state) - dropOff(world, kind)
            if (amount <= 0) return

            var lowest = Int.MAX_VALUE
            val distances = IntArray(OFFSETS.size) { Int.MAX_VALUE }
            val targets = IntArray(OFFSETS.size) { EMPTY }
            for (index in HORIZONTAL) {
                val offset = OFFSETS[index]
                val nextX = x + offset[0]
                val nextZ = z + offset[2]
                val neighbour = at(nextX, y, nextZ)
                if (!canMaybePassThrough(kind, state, neighbour, FACES[index])) continue
                val target = newLiquid(nextX, y, nextZ, kind)
                if (target == EMPTY) continue
                if (!canBeReplacedWith(neighbour, kind, downward = false)) continue
                val distance =
                    if (isHole(nextX, y, nextZ, kind)) 0 else slopeDistance(nextX, y, nextZ, 1, opposite(index), kind)
                distances[index] = distance
                targets[index] = target
                lowest = minOf(lowest, distance)
            }

            for (index in HORIZONTAL) {
                if (targets[index] == EMPTY || distances[index] > lowest) continue
                val offset = OFFSETS[index]
                spreadTo(x + offset[0], y, z + offset[2], kind, downward = false, target = targets[index])
            }
        }

        /** parity: `FlowingFluid.getSlopeDistance`: how far away the nearest drop is, up to the fluid's limit. */
        fun slopeDistance(
            x: Int,
            y: Int,
            z: Int,
            pass: Int,
            from: Int,
            kind: FluidKind,
        ): Int {
            var lowest = SLOPE_MISS
            for (index in HORIZONTAL) {
                if (index == from) continue
                val offset = OFFSETS[index]
                val nextX = x + offset[0]
                val nextZ = z + offset[2]
                val neighbour = at(nextX, y, nextZ)
                if (!canMaybePassThrough(kind, at(x, y, z), neighbour, FACES[index])) continue
                if (isHole(nextX, y, nextZ, kind)) return pass
                if (pass < slopeFindDistance(world, kind)) {
                    lowest = minOf(lowest, slopeDistance(nextX, y, nextZ, pass + 1, opposite(index), kind))
                }
            }
            return lowest
        }

        fun opposite(index: Int): Int = if (index % 2 == 0) index + 1 else index - 1

        /** parity: `FlowingFluid.isWaterHole`: whether the fluid at this position would fall out of it. */
        fun isHole(
            x: Int,
            y: Int,
            z: Int,
            kind: FluidKind,
        ): Boolean {
            val below = at(x, y - 1, z)
            if (!canPassThroughWall(at(x, y, z), below, BlockFace.DOWN)) return false
            return kindOf(below) == kind || canHoldAnyFluid(below)
        }

        fun sourceNeighbourCount(
            x: Int,
            y: Int,
            z: Int,
            kind: FluidKind,
        ): Int {
            var count = 0
            for (index in HORIZONTAL) {
                val offset = OFFSETS[index]
                val neighbour = at(x + offset[0], y + offset[1], z + offset[2])
                if (kindOf(neighbour) == kind && isSource(neighbour)) count++
            }
            return count
        }

        /** parity: `canMaybePassThrough`: a source of the same fluid is never overwritten. */
        fun canMaybePassThrough(
            kind: FluidKind,
            source: Int,
            target: Int,
            face: BlockFace,
        ): Boolean {
            if (kindOf(target) == kind && isSource(target)) return false
            return canHoldAnyFluid(target) && canPassThroughWall(source, target, face)
        }

        /**
         * parity: `FlowingFluid.canPassThroughWall`: the two shapes are merged before asking, so a
         * slab under an upside-down slab walls the flow off even though neither fills its cube.
         */
        fun canPassThroughWall(
            source: Int,
            target: Int,
            face: BlockFace,
        ): Boolean = !BlockShapes.mergedFaceOccludes(source, target, face)

        /** parity: `WaterFluid`/`LavaFluid.canBeReplacedWith`: only lava falling into water, and water into deep lava. */
        fun canBeReplacedWith(
            target: Int,
            incoming: FluidKind,
            downward: Boolean,
        ): Boolean =
            when (kindOf(target)) {
                null -> true
                FluidKind.WATER -> downward && incoming != FluidKind.WATER
                FluidKind.LAVA -> amountOf(target) / 9f >= LAVA_REPLACE_HEIGHT && incoming == FluidKind.WATER
            }

        /** parity: `FlowingFluid.spreadTo`, and `LavaFluid`'s override that makes falling lava fizz into stone. */
        fun spreadTo(
            x: Int,
            y: Int,
            z: Int,
            kind: FluidKind,
            downward: Boolean,
            target: Int,
        ) {
            val existing = at(x, y, z)
            if (downward && kind == FluidKind.LAVA && kindOf(existing) == FluidKind.WATER) {
                set(x, y, z, Blocks.STONE.stateId)
                return
            }
            set(x, y, z, target)
        }

        companion object {
            fun packed(
                x: Int,
                y: Int,
                z: Int,
            ): Long = (x.toLong() and 0x3FFFFFF shl 38) or (y.toLong() and 0xFFF shl 26) or (z.toLong() and 0x3FFFFFF)

            /** parity: the 1000 `getSlopeDistance` starts from, meaning "no drop within reach". */
            const val SLOPE_MISS = 1000
            const val LAVA_REPLACE_HEIGHT = 0.44444445f
        }
    }
}
