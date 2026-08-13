package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.BlockPredicateArgument
import org.basaltmc.server.runtime.command.BlockPredicateInput
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.blocks
import org.basaltmc.wire.text.Component

/**
 * parity: `CloneCommands`, minus its cross-dimension `from`/`to` forms and block entity copying.
 *
 * `force` is what makes an overlapping copy legal, so the source is read whole before anything is
 * written: reading and writing position by position would have a moving region copy itself.
 */
public class CloneCommand : Command(name = "clone", permissionLevel = PermissionLevel.GAMEMASTERS) {
    private enum class Mode { NORMAL, FORCE, MOVE }

    override fun build(root: CommandNodeBuilder) {
        root.argument("begin", BlockPosArgument) {
            argument("end", BlockPosArgument) {
                argument("destination", BlockPosArgument) {
                    executes { context -> clone(context, Mode.NORMAL, null) }
                    for (mode in Mode.entries) {
                        literal(mode.name.lowercase()) { executes { context -> clone(context, mode, null) } }
                    }
                    literal("replace") {
                        executes { context -> clone(context, Mode.NORMAL, null) }
                        for (mode in Mode.entries) {
                            literal(mode.name.lowercase()) { executes { context -> clone(context, mode, null) } }
                        }
                    }
                    literal("masked") {
                        executes { context -> clone(context, Mode.NORMAL, null, skipAir = true) }
                        for (mode in Mode.entries) {
                            literal(mode.name.lowercase()) {
                                executes { context -> clone(context, mode, null, skipAir = true) }
                            }
                        }
                    }
                    literal("filtered") {
                        argument("filter", BlockPredicateArgument) {
                            executes { context -> clone(context, Mode.NORMAL, context.get("filter")) }
                            for (mode in Mode.entries) {
                                literal(mode.name.lowercase()) {
                                    executes { context -> clone(context, mode, context.get("filter")) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun clone(
        context: CommandContext,
        mode: Mode,
        filter: BlockPredicateInput?,
        skipAir: Boolean = false,
    ): Int {
        val editor = context.blocks()
        val origin = context.source.position
        val (beginX, beginY, beginZ) = context.get<BlockCoordinates>("begin").resolve(origin)
        val (endX, endY, endZ) = context.get<BlockCoordinates>("end").resolve(origin)
        val (destX, destY, destZ) = context.get<BlockCoordinates>("destination").resolve(origin)

        val minX = minOf(beginX, endX)
        val minY = minOf(beginY, endY)
        val minZ = minOf(beginZ, endZ)
        val maxX = maxOf(beginX, endX)
        val maxY = maxOf(beginY, endY)
        val maxZ = maxOf(beginZ, endZ)
        val spanX = maxX - minX + 1
        val spanY = maxY - minY + 1
        val spanZ = maxZ - minZ + 1

        val volume = spanX.toLong() * spanY * spanZ
        if (volume > MAX_BLOCK_MODIFICATIONS) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.clone.toobig",
                    Component.text(MAX_BLOCK_MODIFICATIONS.toString()),
                    Component.text(volume.toString()),
                ),
            )
        }

        val overlaps =
            destX <= maxX &&
                destX + spanX > minX &&
                destY <= maxY &&
                destY + spanY > minY &&
                destZ <= maxZ &&
                destZ + spanZ > minZ
        if (overlaps && mode != Mode.FORCE) {
            throw CommandSyntaxException(Component.translatable("commands.clone.overlap"))
        }

        editor.require(minX, minY, minZ)
        editor.require(maxX, maxY, maxZ)
        editor.require(destX, destY, destZ)
        editor.require(destX + spanX - 1, destY + spanY - 1, destZ + spanZ - 1)

        // Read first, write after: a `force` clone onto itself would otherwise copy blocks it has
        // already overwritten.
        val copied = ArrayList<Triple<Int, Int, Int>>(spanX * spanY * spanZ)
        val states = ArrayList<Int>(copied.size)
        for (x in 0 until spanX) {
            for (y in 0 until spanY) {
                for (z in 0 until spanZ) {
                    val state = editor.stateAt(minX + x, minY + y, minZ + z) ?: continue
                    if (skipAir && state == Blocks.AIR.stateId) continue
                    if (filter != null && !filter.matches(state)) continue
                    copied += Triple(x, y, z)
                    states += state
                }
            }
        }

        var count = 0
        if (mode == Mode.MOVE) {
            for (index in copied.indices) {
                val (x, y, z) = copied[index]
                editor.set(minX + x, minY + y, minZ + z, Blocks.AIR.stateId)
            }
        }
        for (index in copied.indices) {
            val (x, y, z) = copied[index]
            if (editor.set(destX + x, destY + y, destZ + z, states[index])) count++
        }
        editor.flush()

        if (count == 0) throw CommandSyntaxException(Component.translatable("commands.clone.failed"))
        context.success(
            Component.translatable("commands.clone.success", Component.text(count.toString())),
            broadcastToAdmins = true,
        )
        return count
    }

    private companion object {
        const val MAX_BLOCK_MODIFICATIONS = 32768L
    }
}
