package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.PushReaction
import org.basaltmc.wire.common.Identifier

/**
 * What a piston can move, and what happens when it does.
 *
 * parity: `PistonStructureResolver` for the shape of the push (a line of at most twelve, plus every
 * branch a slime block drags with it) and `PistonBaseBlock.isPushable` for what refuses to move at
 * all. Obsidian stops it, a torch breaks, bedrock is unbreakable and a block that is `IGNORE` is
 * left where it stands.
 *
 * The two-tick slide is not modelled: vanilla puts a `moving_piston` block entity in the way and
 * animates it, and here the blocks arrive at once. What moves and what refuses is the part parity
 * is built on, and that is what this answers.
 */
public object Pistons {
    /** parity: `PistonStructureResolver`'s own twelve. */
    public const val PUSH_LIMIT: Int = 12

    /** Where a push ends up: what moves, in the order it has to be written, and what breaks. */
    public class Push(
        public val moved: List<IntArray>,
        public val destroyed: List<IntArray>,
    )

    /**
     * parity: `PistonBaseBlock.isPushable`. Air moves, obsidian and its family never do, a block
     * with no destroy speed (bedrock) never does, and the push reaction decides the rest.
     */
    public fun isPushable(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        pushing: BlockFace,
        allowDestroyable: Boolean,
    ): Boolean {
        if (y < world.minY || y > world.maxY) return false
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        if (stateId == Blocks.AIR.stateId) return true
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        if (path in IMMOVABLE) return false
        if (pushing == BlockFace.DOWN && y == world.minY) return false
        if (pushing == BlockFace.UP && y == world.maxY) return false
        if (path != "piston" && path != "sticky_piston") {
            if (Blocks.hardnessOf(Identifier.vanilla(path))?.isUnbreakable == true) return false
            when (BlockSupport.pushReaction(stateId)) {
                PushReaction.BLOCK -> return false
                PushReaction.DESTROY -> return allowDestroyable
                PushReaction.PUSH_ONLY -> return pushing == pistonFacingOf(stateId)
                else -> Unit
            }
        } else if (BlockStateDefinitions.propertiesOf(stateId)["extended"] == "true") {
            // An extended piston is holding its own head: it is not going anywhere.
            return false
        }
        // parity: the last line of `isPushable`. A chest is `NORMAL` and still will not move, because
        // what it holds lives in a block entity and a piston does not carry those.
        return !BlockConnections.hasBlockEntity(stateId)
    }

    private fun pistonFacingOf(stateId: Int): BlockFace? =
        BlockStateDefinitions.propertiesOf(stateId)["facing"]?.let { facing ->
            BlockFace.entries.firstOrNull { it.name.lowercase() == facing }
        }

    /**
     * Works out the push a piston at these coordinates would make, or null when it is blocked.
     *
     * parity: `PistonStructureResolver.resolve`, the line in front of the head plus every branch a
     * sticky block drags along, refusing altogether past twelve blocks.
     */
    public fun resolve(
        world: LoadedWorld,
        pistonX: Int,
        pistonY: Int,
        pistonZ: Int,
        pushing: BlockFace,
        extending: Boolean,
    ): Push? {
        val piston = intArrayOf(pistonX, pistonY, pistonZ)
        val start = intArrayOf(pistonX + pushing.stepX, pistonY + pushing.stepY, pistonZ + pushing.stepZ)
        val resolver = Resolver(world, piston, pushing)
        val startState = world.state.getBlockState(start[0], start[1], start[2]) ?: return null
        if (!isPushable(world, start[0], start[1], start[2], pushing, allowDestroyable = false)) {
            if (extending && BlockSupport.pushReaction(startState) == PushReaction.DESTROY) {
                resolver.destroyed += start
                return Push(emptyList(), resolver.destroyed)
            }
            return null
        }
        if (!resolver.addLine(start, pushing)) return null
        var index = 0
        while (index < resolver.toPush.size) {
            val position = resolver.toPush[index]
            val stateId = world.state.getBlockState(position[0], position[1], position[2])
            if (stateId != null && isSticky(stateId) && !resolver.addBranches(position)) return null
            index++
        }
        return Push(resolver.toPush.toList(), resolver.destroyed)
    }

    /** parity: `Blocks.SLIME_BLOCK` and `Blocks.HONEY_BLOCK`, the two that carry their neighbours. */
    private fun isSticky(stateId: Int): Boolean =
        BlockStateDefinitions.blockOf(stateId)?.path.let { it == "slime_block" || it == "honey_block" }

    /** parity: `canStickToEachOther`: slime and honey each stick to anything but each other. */
    private fun sticksTo(
        one: Int,
        other: Int,
    ): Boolean {
        val a = BlockStateDefinitions.blockOf(one)?.path
        val b = BlockStateDefinitions.blockOf(other)?.path
        if (a == "honey_block" && b == "slime_block") return false
        if (a == "slime_block" && b == "honey_block") return false
        return isSticky(one) || isSticky(other)
    }

    /** One push being worked out. parity: `PistonStructureResolver`'s own two lists. */
    private class Resolver(
        val world: LoadedWorld,
        val piston: IntArray,
        val pushing: BlockFace,
    ) {
        val toPush = mutableListOf<IntArray>()
        val destroyed = mutableListOf<IntArray>()

        fun indexOf(position: IntArray): Int =
            toPush.indexOfFirst { it[0] == position[0] && it[1] == position[1] && it[2] == position[2] }

        fun stateAt(position: IntArray): Int? = world.state.getBlockState(position[0], position[1], position[2])

        fun step(
            from: IntArray,
            face: BlockFace,
            times: Int = 1,
        ): IntArray =
            intArrayOf(from[0] + face.stepX * times, from[1] + face.stepY * times, from[2] + face.stepZ * times)

        fun isPiston(position: IntArray): Boolean =
            position[0] == piston[0] && position[1] == piston[1] && position[2] == piston[2]

        /** parity: `addBlockLine`: what a sticky block drags behind it, then the line ahead of it. */
        fun addLine(
            start: IntArray,
            direction: BlockFace,
        ): Boolean {
            var nextState = stateAt(start) ?: return true
            if (nextState == Blocks.AIR.stateId) return true
            if (!isPushable(world, start[0], start[1], start[2], pushing, allowDestroyable = false)) return true
            if (isPiston(start)) return true
            if (indexOf(start) > -1) return true
            var blockCount = 1
            if (blockCount + toPush.size > PUSH_LIMIT) return false
            while (isSticky(nextState)) {
                val behind = step(start, pushing.opposite, blockCount)
                val previous = nextState
                nextState = stateAt(behind) ?: break
                if (nextState == Blocks.AIR.stateId ||
                    !sticksTo(previous, nextState) ||
                    !isPushable(world, behind[0], behind[1], behind[2], pushing, allowDestroyable = false) ||
                    isPiston(behind)
                ) {
                    break
                }
                blockCount++
                if (blockCount + toPush.size > PUSH_LIMIT) return false
            }
            var blocksAdded = 0
            for (index in blockCount - 1 downTo 0) {
                toPush += step(start, pushing.opposite, index)
                blocksAdded++
            }
            var ahead = 1
            while (true) {
                val position = step(start, pushing, ahead)
                val collision = indexOf(position)
                if (collision > -1) {
                    reorderAtCollision(blocksAdded, collision)
                    for (index in 0..collision + blocksAdded) {
                        val moved = toPush.getOrNull(index) ?: break
                        val movedState = stateAt(moved) ?: continue
                        if (isSticky(movedState) && !addBranches(moved)) return false
                    }
                    return true
                }
                val state = stateAt(position) ?: return true
                if (state == Blocks.AIR.stateId) return true
                if (!isPushable(world, position[0], position[1], position[2], pushing, allowDestroyable = true) ||
                    isPiston(position)
                ) {
                    return false
                }
                if (BlockSupport.pushReaction(state) == PushReaction.DESTROY) {
                    destroyed += position
                    return true
                }
                if (toPush.size >= PUSH_LIMIT) return false
                toPush += position
                ahead++
            }
        }

        /** parity: `addBranchingBlocks`: every side a sticky block reaches, off the push axis. */
        fun addBranches(from: IntArray): Boolean {
            val fromState = stateAt(from) ?: return true
            for (face in BlockFace.entries) {
                if (face.axis == pushing.axis) continue
                val neighbour = step(from, face)
                val neighbourState = stateAt(neighbour) ?: continue
                if (!sticksTo(neighbourState, fromState)) continue
                if (!addLine(neighbour, face)) return false
            }
            return true
        }

        /** parity: `reorderListAtCollision`, which puts the line that just arrived ahead of the rest. */
        fun reorderAtCollision(
            blocksAdded: Int,
            collision: Int,
        ) {
            val head = toPush.subList(0, collision).toList()
            val lastLine = toPush.subList(toPush.size - blocksAdded, toPush.size).toList()
            val middle = toPush.subList(collision, toPush.size - blocksAdded).toList()
            toPush.clear()
            toPush += head
            toPush += lastLine
            toPush += middle
        }
    }

    /**
     * Carries the push out: every block moves one along, back to front so nothing overwrites the one
     * behind it, and what the push destroyed goes.
     */
    public fun apply(
        world: LoadedWorld,
        push: Push,
        pushing: BlockFace,
    ): List<BlockChange> {
        val changes = mutableListOf<BlockChange>()
        for (position in push.destroyed) {
            if (!world.state.setBlockState(position[0], position[1], position[2], Blocks.AIR.stateId)) continue
            changes += BlockChange(position[0], position[1], position[2], Blocks.AIR.stateId)
        }
        for (position in push.moved.asReversed()) {
            val stateId = world.state.getBlockState(position[0], position[1], position[2]) ?: continue
            val toX = position[0] + pushing.stepX
            val toY = position[1] + pushing.stepY
            val toZ = position[2] + pushing.stepZ
            if (world.state.setBlockState(toX, toY, toZ, stateId)) changes += BlockChange(toX, toY, toZ, stateId)
            if (world.state.setBlockState(position[0], position[1], position[2], Blocks.AIR.stateId)) {
                changes += BlockChange(position[0], position[1], position[2], Blocks.AIR.stateId)
            }
        }
        return changes
    }

    /** parity: the four blocks `isPushable` names one by one before it looks at anything else. */
    private val IMMOVABLE =
        setOf("obsidian", "crying_obsidian", "respawn_anchor", "reinforced_deepslate")
}
