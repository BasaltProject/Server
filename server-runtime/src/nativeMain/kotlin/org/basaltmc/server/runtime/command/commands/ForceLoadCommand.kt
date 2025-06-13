package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.ColumnCoordinates
import org.basaltmc.server.runtime.command.ColumnPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.wire.text.Component

/**
 * parity: `ForceLoadCommand`, whose coordinates are *block* coordinates that name a chunk, which is
 * why `/forceload add 0 0` forces chunk 0,0 and `/forceload add 16 16` forces chunk 1,1.
 */
public class ForceLoadCommand : Command(name = "forceload", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("add") {
            argument("from", ColumnPosArgument) {
                executes { context -> add(context, single = true) }
                argument("to", ColumnPosArgument) { executes { context -> add(context, single = false) } }
            }
        }
        root.literal("remove") {
            literal("all") { executes { context -> removeAll(context) } }
            argument("from", ColumnPosArgument) {
                executes { context -> remove(context, single = true) }
                argument("to", ColumnPosArgument) { executes { context -> remove(context, single = false) } }
            }
        }
        root.literal("query") {
            executes { context -> queryAll(context) }
            argument("pos", ColumnPosArgument) { executes { context -> query(context) } }
        }
    }

    private fun chunkRange(
        context: CommandContext,
        single: Boolean,
    ): List<Pair<Int, Int>> {
        val origin = context.source.position
        val (fromX, fromZ) = context.get<ColumnCoordinates>("from").resolve(origin)
        val (toX, toZ) = if (single) fromX to fromZ else context.get<ColumnCoordinates>("to").resolve(origin)
        val minX = minOf(fromX, toX) shr 4
        val maxX = maxOf(fromX, toX) shr 4
        val minZ = minOf(fromZ, toZ) shr 4
        val maxZ = maxOf(fromZ, toZ) shr 4
        val count = (maxX - minX + 1).toLong() * (maxZ - minZ + 1)
        if (count > MAX_CHUNKS) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.forceload.toobig",
                    Component.text(MAX_CHUNKS.toString()),
                    Component.text(count.toString()),
                ),
            )
        }
        return (minX..maxX).flatMap { x -> (minZ..maxZ).map { z -> x to z } }
    }

    private fun add(
        context: CommandContext,
        single: Boolean,
    ): Int {
        val world = context.world()
        val chunks = chunkRange(context, single)
        val added = chunks.filter { world.forcedChunks.add(ChunkGenerationQueue.pack(it.first, it.second)) }
        if (added.isEmpty()) {
            throw CommandSyntaxException(Component.translatable("commands.forceload.added.failure"))
        }
        // Generated now, not on next visit: the point of forcing a chunk is that it is there.
        for ((x, z) in added) world.generationQueue.request(x, z, priority = 0)
        context.success(
            if (added.size == 1) {
                val (x, z) = added.single()
                Component.translatable(
                    "commands.forceload.added.single",
                    Component.text("$x, $z"),
                    Component.text(world.name),
                )
            } else {
                Component.translatable(
                    "commands.forceload.added.multiple",
                    Component.text(added.size.toString()),
                    Component.text(world.name),
                    Component.text("${added.first()}"),
                    Component.text("${added.last()}"),
                )
            },
            broadcastToAdmins = true,
        )
        return added.size
    }

    private fun remove(
        context: CommandContext,
        single: Boolean,
    ): Int {
        val world = context.world()
        val chunks = chunkRange(context, single)
        val removed = chunks.filter { world.forcedChunks.remove(ChunkGenerationQueue.pack(it.first, it.second)) }
        if (removed.isEmpty()) {
            throw CommandSyntaxException(Component.translatable("commands.forceload.removed.failure"))
        }
        context.success(
            if (removed.size == 1) {
                val (x, z) = removed.single()
                Component.translatable(
                    "commands.forceload.removed.single",
                    Component.text("$x, $z"),
                    Component.text(world.name),
                )
            } else {
                Component.translatable(
                    "commands.forceload.removed.multiple",
                    Component.text(removed.size.toString()),
                    Component.text(world.name),
                    Component.text("${removed.first()}"),
                    Component.text("${removed.last()}"),
                )
            },
            broadcastToAdmins = true,
        )
        return removed.size
    }

    private fun removeAll(context: CommandContext): Int {
        val world = context.world()
        world.forcedChunks.clear()
        context.success(
            Component.translatable("commands.forceload.removed.all", Component.text(world.name)),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun query(context: CommandContext): Int {
        val world = context.world()
        val (x, z) = context.get<ColumnCoordinates>("pos").resolve(context.source.position)
        val chunkX = x shr 4
        val chunkZ = z shr 4
        val forced = ChunkGenerationQueue.pack(chunkX, chunkZ) in world.forcedChunks
        if (!forced) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.forceload.query.failure",
                    Component.text("$chunkX, $chunkZ"),
                    Component.text(world.name),
                ),
            )
        }
        context.success(
            Component.translatable(
                "commands.forceload.query.success",
                Component.text("$chunkX, $chunkZ"),
                Component.text(world.name),
            ),
        )
        return 1
    }

    private fun queryAll(context: CommandContext): Int {
        val world = context.world()
        val forced = world.forcedChunks
        if (forced.isEmpty()) {
            throw CommandSyntaxException(
                Component.translatable("commands.forceload.added.none", Component.text(world.name)),
            )
        }
        context.success(
            Component.translatable(
                "commands.forceload.list.multiple",
                Component.text(forced.size.toString()),
                Component.text(world.name),
                Component.text(
                    forced.joinToString(", ") { key ->
                        "${ChunkGenerationQueue.unpackX(key)}, ${ChunkGenerationQueue.unpackZ(key)}"
                    },
                ),
            ),
        )
        return forced.size
    }

    private companion object {
        /** parity: vanilla's own cap on one `/forceload add`. */
        const val MAX_CHUNKS = 256L
    }
}
