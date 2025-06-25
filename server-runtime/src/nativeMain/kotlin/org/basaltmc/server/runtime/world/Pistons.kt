package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.PushReaction
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * What a piston can move, and what happens when it does.
 *
 * parity: `PistonStructureResolver` for the shape of the push (a line of at most twelve, plus every
 * branch a slime block drags with it) and `PistonBaseBlock.isPushable` for what refuses to move at
 * all. Obsidian stops it, a torch breaks, bedrock is unbreakable and a block that is `IGNORE` is
 * left where it stands.
 *
 * The two-tick slide is a `moving_piston` carrying the state it will become, as in vanilla; what is
 * not drawn is the sliding itself, only the tick it lands on.
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

    public val PISTON: Identifier = Identifier.vanilla("piston")

    public val MOVING_PISTON: Identifier = Identifier.vanilla("piston")

    /** parity: `PistonBaseBlock`'s own two ticks between the arm starting to move and the blocks landing. */
    public const val MOVE_TICKS: Int = 2

    /** parity: `PistonBaseBlock.TRIGGER_EXTEND` and `TRIGGER_CONTRACT`. */
    private const val EXTEND = 0
    private const val CONTRACT = 1

    /**
     * parity: `PistonBaseBlock.checkIfExtend`: a piston extends when it is powered from anywhere but
     * its own face, and pulls back when that power goes.
     *
     * The move itself lands two ticks later, which is when vanilla's `moving_piston` finishes its
     * slide. The head sliding is the client's own half, from the block event this sends.
     */
    public fun trigger(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        // parity: `Level.blockEvent`, which is the only thing that makes a piston *look* like it
        // moves: the head slides on the client while the server takes its two ticks to move blocks.
        effects: MutableList<BlockChange> = mutableListOf(),
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return
        if (path != "piston" && path != "sticky_piston") return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = properties["facing"] ?: return
        val direction = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return
        val extended = properties["extended"] == "true"
        val powered = signalToPiston(world, x, y, z, direction)
        if (powered == extended) return
        if (powered) {
            val push = resolve(world, x, y, z, direction, extending = true) ?: return
            beginMove(world, push, direction, sticky = path == "sticky_piston")
        }
        // parity: `PistonBaseBlock.checkIfExtend`: 0 to push out, 1 to pull back, and the face it
        // moves along as the second byte.
        effects +=
            BlockChange(
                x,
                y,
                z,
                stateId,
                listOf(BlockEffect.Event(b0 = if (powered) EXTEND else CONTRACT, b1 = direction.ordinal)),
            )
        world.state.scheduleBlockTick(x, y, z, PISTON, MOVE_TICKS)
    }

    /**
     * parity: `PistonBaseBlock.getNeighborSignal`, which ignores its own face: a piston cannot be
     * powered by what it is about to push.
     */
    private fun signalToPiston(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        facing: BlockFace,
    ): Boolean {
        for (face in BlockFace.entries) {
            if (face == facing) continue
            val nx = x + face.stepX
            val ny = y + face.stepY
            val nz = z + face.stepZ
            if (Redstone.signalFrom(world, nx, ny, nz, face) > 0) return true
        }
        // parity: the block above is asked twice, once for itself and once for what is over it,
        // which is the quirk that makes a piston fire from a signal two blocks up.
        if (Redstone.signalFrom(world, x, y + 1, z, BlockFace.UP) > 0) return true
        for (face in BlockFace.entries) {
            if (face == BlockFace.DOWN) continue
            val nx = x + face.stepX
            val ny = y + 1 + face.stepY
            val nz = z + face.stepZ
            if (Redstone.signalFrom(world, nx, ny, nz, face) > 0) return true
        }
        return false
    }

    /**
     * The move landing: the blocks go one along, the head appears or goes, and a sticky piston takes
     * the block in front of it back with it.
     */
    public fun finish(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        players: List<PlayerPosition> = emptyList(),
        onPlayerPushed: (PlayerPosition) -> Unit = {},
    ): List<BlockChange> {
        val stateId = world.state.getBlockState(x, y, z) ?: return emptyList()
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return emptyList()
        if (path != "piston" && path != "sticky_piston") return emptyList()
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = properties["facing"] ?: return emptyList()
        val direction = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return emptyList()
        val extended = properties["extended"] == "true"
        val powered = signalToPiston(world, x, y, z, direction)
        if (powered == extended) return emptyList()
        return if (powered) {
            extend(
                world,
                x,
                y,
                z,
                stateId,
                direction,
                path,
                players,
                onPlayerPushed,
            )
        } else {
            retract(world, x, y, z, stateId, direction, path)
        }
    }

    /**
     * parity: `MovingPistonBlock` and its block entity: while the arm slides, each block on the way
     * is a `moving_piston` carrying the state it will become. That is what a client draws mid-slide,
     * and it is why a block being pushed is not solid for those two ticks.
     */
    private fun beginMove(
        world: LoadedWorld,
        push: Push,
        direction: BlockFace,
        sticky: Boolean,
    ) {
        val moving =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla("moving_piston"),
                mapOf("facing" to direction.name.lowercase(), "type" to if (sticky) "sticky" else "normal"),
            ) ?: return
        for (position in push.moved) {
            val carried = world.state.getBlockState(position[0], position[1], position[2]) ?: continue
            val chunk = world.state.chunkIfLoaded(position[0] shr 4, position[2] shr 4) ?: continue
            if (!world.state.setBlockState(position[0], position[1], position[2], moving)) continue
            val data = NbtCompound()
            data.putInt("blockState", carried)
            data.putString("facing", direction.name.lowercase())
            data.putBoolean("extending", true)
            data.putBoolean("source", false)
            chunk.setBlockEntity(
                ChunkBlockEntity(position[0] and 15, position[1], position[2] and 15, MOVING_PISTON, data),
            )
        }
    }

    private fun extend(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        direction: BlockFace,
        path: String,
        players: List<PlayerPosition>,
        onPlayerPushed: (PlayerPosition) -> Unit,
    ): List<BlockChange> {
        val changes = mutableListOf<BlockChange>()
        // The blocks are `moving_piston` right now, each carrying what it is about to become.
        val carried = carriedBlocks(world, x, y, z, direction)
        // Whatever they sweep comes along, which is `moveCollidedEntities`.
        dragAlong(world, carried.map { it.first }, direction, players, onPlayerPushed)
        changes += landCarried(world, carried, direction)
        // parity: the arm, which is a block of its own and carries whether the piston is sticky.
        val headX = x + direction.stepX
        val headY = y + direction.stepY
        val headZ = z + direction.stepZ
        val head =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla("piston_head"),
                mapOf(
                    "facing" to direction.name.lowercase(),
                    "type" to if (path == "sticky_piston") "sticky" else "normal",
                    "short" to "false",
                ),
            )
        if (head != null && world.state.setBlockState(headX, headY, headZ, head)) {
            changes += BlockChange(headX, headY, headZ, head)
        }
        val body = BlockStateDefinitions.stateIdOf(Identifier.vanilla(path), mapOf("extended" to "true"), stateId)
        if (body != null && world.state.setBlockState(x, y, z, body)) changes += BlockChange(x, y, z, body)
        return changes
    }

    /**
     * The blocks mid-slide: every `moving_piston` around the piston, with the state it carries,
     * furthest along first so nothing lands on a block that has not moved yet.
     */
    private fun carriedBlocks(
        world: LoadedWorld,
        pistonX: Int,
        pistonY: Int,
        pistonZ: Int,
        direction: BlockFace,
    ): List<Pair<IntArray, Int>> {
        // A push reaches twelve blocks plus its branches, so the chunk the piston is in and the
        // eight around it hold every block that is on the move.
        val carried = mutableListOf<Pair<IntArray, Int>>()
        for (offsetZ in -1..1) {
            for (offsetX in -1..1) {
                val chunk =
                    world.state.chunkIfLoaded((pistonX shr 4) + offsetX, (pistonZ shr 4) + offsetZ) ?: continue
                for (entity in chunk.blockEntities()) {
                    if (entity.type != MOVING_PISTON) continue
                    val state = (entity.data["blockState"] as? org.basaltmc.wire.nbt.NbtInt)?.value ?: continue
                    val x = ((chunk.chunkX) shl 4) + entity.localX
                    val z = ((chunk.chunkZ) shl 4) + entity.localZ
                    if (kotlin.math.abs(x - pistonX) > PUSH_LIMIT + 1) continue
                    if (kotlin.math.abs(entity.y - pistonY) > PUSH_LIMIT + 1) continue
                    if (kotlin.math.abs(z - pistonZ) > PUSH_LIMIT + 1) continue
                    carried += intArrayOf(x, entity.y, z) to state
                }
            }
        }
        carried.sortByDescending {
            it.first[0] * direction.stepX + it.first[1] * direction.stepY + it.first[2] * direction.stepZ
        }
        return carried
    }

    /**
     * Puts every carried block down one along, which is what `PistonMovingBlockEntity.finalTick`
     * does when the slide ends.
     */
    private fun landCarried(
        world: LoadedWorld,
        carried: List<Pair<IntArray, Int>>,
        direction: BlockFace,
    ): List<BlockChange> {
        val changes = mutableListOf<BlockChange>()
        for ((position, stateId) in carried) {
            val toX = position[0] + direction.stepX
            val toY = position[1] + direction.stepY
            val toZ = position[2] + direction.stepZ
            if (world.state.setBlockState(position[0], position[1], position[2], Blocks.AIR.stateId)) {
                changes += BlockChange(position[0], position[1], position[2], Blocks.AIR.stateId)
            }
            if (world.state.setBlockState(toX, toY, toZ, stateId)) changes += BlockChange(toX, toY, toZ, stateId)
        }
        return changes
    }

    /** parity: a sticky piston brings one block back with it; a plain one leaves it where it is. */
    private fun retract(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        direction: BlockFace,
        path: String,
    ): List<BlockChange> {
        val changes = mutableListOf<BlockChange>()
        val headX = x + direction.stepX
        val headY = y + direction.stepY
        val headZ = z + direction.stepZ
        if (world.state.setBlockState(headX, headY, headZ, Blocks.AIR.stateId)) {
            changes += BlockChange(headX, headY, headZ, Blocks.AIR.stateId)
        }
        if (path == "sticky_piston") {
            val pulledX = headX + direction.stepX
            val pulledY = headY + direction.stepY
            val pulledZ = headZ + direction.stepZ
            val pulled = world.state.getBlockState(pulledX, pulledY, pulledZ)
            if (pulled != null &&
                pulled != Blocks.AIR.stateId &&
                isPushable(world, pulledX, pulledY, pulledZ, direction.opposite, allowDestroyable = false)
            ) {
                if (world.state.setBlockState(headX, headY, headZ, pulled)) {
                    changes += BlockChange(headX, headY, headZ, pulled)
                }
                if (world.state.setBlockState(pulledX, pulledY, pulledZ, Blocks.AIR.stateId)) {
                    changes += BlockChange(pulledX, pulledY, pulledZ, Blocks.AIR.stateId)
                }
            }
        }
        val body = BlockStateDefinitions.stateIdOf(Identifier.vanilla(path), mapOf("extended" to "false"), stateId)
        if (body != null && world.state.setBlockState(x, y, z, body)) changes += BlockChange(x, y, z, body)
        return changes
    }

    /**
     * Moves whatever the pushed blocks sweep over.
     *
     * parity: `PistonMovingBlockEntity.moveCollidedEntities`, which shifts every entity inside the
     * area a moving block covers on its way. Here that is the players and the dropped items; a
     * player is handed back through [onPlayerPushed], because moving one means telling its client.
     */
    public fun dragAlong(
        world: LoadedWorld,
        moving: List<IntArray>,
        direction: BlockFace,
        players: List<PlayerPosition> = emptyList(),
        onPlayerPushed: (PlayerPosition) -> Unit = {},
    ) {
        if (moving.isEmpty()) return
        val items = world.state.items()
        for (position in moving) {
            for (item in items) {
                if (!inSweptArea(item.x, item.y, item.z, position, direction)) continue
                item.x += direction.stepX
                item.y += direction.stepY
                item.z += direction.stepZ
            }
            for (player in players) {
                if (!inSweptArea(player.x, player.y, player.z, position, direction)) continue
                onPlayerPushed(
                    PlayerPosition(
                        player.uuid,
                        player.x + direction.stepX,
                        player.y + direction.stepY,
                        player.z + direction.stepZ,
                    ),
                )
            }
        }
    }

    /** Somebody the piston has to shove, and where they end up. */
    public class PlayerPosition(
        public val uuid: String,
        public val x: Double,
        public val y: Double,
        public val z: Double,
    )

    /** The box a moving block sweeps: where it is now and where it lands. */
    private fun inSweptArea(
        x: Double,
        y: Double,
        z: Double,
        block: IntArray,
        direction: BlockFace,
    ): Boolean {
        val fromX = block[0]
        val fromY = block[1]
        val fromZ = block[2]
        val toX = fromX + direction.stepX
        val toY = fromY + direction.stepY
        val toZ = fromZ + direction.stepZ
        val minX = minOf(fromX, toX)
        val minY = minOf(fromY, toY)
        val minZ = minOf(fromZ, toZ)
        val maxX = maxOf(fromX, toX) + 1
        val maxY = maxOf(fromY, toY) + 1
        val maxZ = maxOf(fromZ, toZ) + 1
        return x >= minX && x < maxX && y >= minY && y < maxY && z >= minZ && z < maxZ
    }

    /** parity: the four blocks `isPushable` names one by one before it looks at anything else. */
    private val IMMOVABLE =
        setOf("obsidian", "crying_obsidian", "respawn_anchor", "reinforced_deepslate")
}
