package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockEditor
import org.basaltmc.server.runtime.command.BlockInput
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.BlockPredicateArgument
import org.basaltmc.server.runtime.command.BlockPredicateInput
import org.basaltmc.server.runtime.command.BlockStateArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.blocks
import org.basaltmc.wire.text.Component

/**
 * parity: `FillCommand`, including its five modes and the `replace <filter>` form.
 *
 * The cap is vanilla's `maxCommandChainLength`-sibling game rule `maxBlockModifications`, whose
 * default is 32768. Game rules are not configurable yet, so the default is the value.
 */
public class FillCommand : Command(name = "fill", permissionLevel = PermissionLevel.GAMEMASTERS) {
    private enum class Mode { REPLACE, OUTLINE, HOLLOW, DESTROY }

    override fun build(root: CommandNodeBuilder) {
        root.argument("from", BlockPosArgument) {
            argument("to", BlockPosArgument) {
                argument("block", BlockStateArgument) {
                    executes { context -> fill(context, Mode.REPLACE, null) }
                    literal("outline") { executes { context -> fill(context, Mode.OUTLINE, null) } }
                    literal("hollow") { executes { context -> fill(context, Mode.HOLLOW, null) } }
                    literal("destroy") { executes { context -> fill(context, Mode.DESTROY, null) } }
                    literal("keep") {
                        executes { context -> fill(context, Mode.REPLACE, null, onlyAir = true) }
                    }
                    // parity: `strict` writes the state as written; nothing here adjusts one anyway.
                    literal("strict") { executes { context -> fill(context, Mode.REPLACE, null) } }
                    literal("replace") {
                        executes { context -> fill(context, Mode.REPLACE, null) }
                        argument("filter", BlockPredicateArgument) {
                            executes { context -> fill(context, Mode.REPLACE, context.get("filter")) }
                            literal("strict") {
                                executes { context ->
                                    fill(context, Mode.REPLACE, context.get("filter"))
                                }
                            }
                            literal("outline") {
                                executes { context -> fill(context, Mode.OUTLINE, context.get("filter")) }
                            }
                            literal("hollow") {
                                executes { context -> fill(context, Mode.HOLLOW, context.get("filter")) }
                            }
                            literal("destroy") {
                                executes { context -> fill(context, Mode.DESTROY, context.get("filter")) }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun fill(
        context: CommandContext,
        mode: Mode,
        filter: BlockPredicateInput?,
        onlyAir: Boolean = false,
    ): Int {
        val editor = context.blocks()
        val origin = context.source.position
        val (fromX, fromY, fromZ) = context.get<BlockCoordinates>("from").resolve(origin)
        val (toX, toY, toZ) = context.get<BlockCoordinates>("to").resolve(origin)
        val minX = minOf(fromX, toX)
        val minY = minOf(fromY, toY)
        val minZ = minOf(fromZ, toZ)
        val maxX = maxOf(fromX, toX)
        val maxY = maxOf(fromY, toY)
        val maxZ = maxOf(fromZ, toZ)

        val volume = (maxX - minX + 1).toLong() * (maxY - minY + 1) * (maxZ - minZ + 1)
        if (volume > MAX_BLOCK_MODIFICATIONS) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.fill.toobig",
                    Component.text(MAX_BLOCK_MODIFICATIONS.toString()),
                    Component.text(volume.toString()),
                ),
            )
        }
        editor.require(minX, minY, minZ)
        editor.require(maxX, maxY, maxZ)

        val block = context.get<BlockInput>("block")
        var count = 0
        for (x in minX..maxX) {
            for (y in minY..maxY) {
                for (z in minZ..maxZ) {
                    val existing = editor.stateAt(x, y, z) ?: continue
                    if (filter != null && !filter.matches(existing)) continue
                    if (onlyAir && existing != Blocks.AIR.stateId) continue
                    val onEdge =
                        x == minX || x == maxX || y == minY || y == maxY || z == minZ || z == maxZ
                    val target =
                        when {
                            mode == Mode.OUTLINE && !onEdge -> continue
                            mode == Mode.HOLLOW && !onEdge -> Blocks.AIR.stateId
                            else -> block.stateId
                        }
                    if (writeOne(editor, x, y, z, target, mode)) count++
                }
            }
        }
        editor.flush()

        if (count == 0) throw CommandSyntaxException(Component.translatable("commands.fill.failed"))
        context.success(
            Component.translatable("commands.fill.success", Component.text(count.toString())),
            broadcastToAdmins = true,
        )
        return count
    }

    private fun writeOne(
        editor: BlockEditor,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        mode: Mode,
    ): Boolean {
        // parity: DESTROY breaks the old block first, so the count includes a position whose new
        // state happens to equal the old one.
        if (mode == Mode.DESTROY) {
            val cleared = editor.set(x, y, z, Blocks.AIR.stateId)
            return editor.set(x, y, z, stateId) || cleared
        }
        return editor.set(x, y, z, stateId)
    }

    private companion object {
        /** parity: the `maxBlockModifications` game rule's default. */
        const val MAX_BLOCK_MODIFICATIONS = 32768L
    }
}
