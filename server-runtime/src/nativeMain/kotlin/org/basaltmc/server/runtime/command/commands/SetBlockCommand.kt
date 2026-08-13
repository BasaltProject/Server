package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockInput
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.BlockStateArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.blocks
import org.basaltmc.wire.text.Component

/**
 * parity: `SetBlockCommand`.
 *
 * `destroy` drops nothing yet: block drops go through the loot path, which needs the block entity
 * and tool context a command does not have. It clears the block like vanilla does and says nothing
 * else, which is the same visible result minus the item entity.
 */
public class SetBlockCommand : Command(name = "setblock", permissionLevel = PermissionLevel.GAMEMASTERS) {
    private enum class Mode { REPLACE, DESTROY, KEEP }

    override fun build(root: CommandNodeBuilder) {
        root.argument("pos", BlockPosArgument) {
            argument("block", BlockStateArgument) {
                executes { context -> setBlock(context, Mode.REPLACE) }
                literal("destroy") { executes { context -> setBlock(context, Mode.DESTROY) } }
                literal("keep") { executes { context -> setBlock(context, Mode.KEEP) } }
                literal("replace") { executes { context -> setBlock(context, Mode.REPLACE) } }
            }
        }
    }

    private fun setBlock(
        context: CommandContext,
        mode: Mode,
    ): Int {
        val editor = context.blocks()
        val (x, y, z) = context.get<BlockCoordinates>("pos").resolve(context.source.position)
        editor.require(x, y, z)
        val block = context.get<BlockInput>("block")

        if (mode == Mode.KEEP && editor.stateAt(x, y, z) != Blocks.AIR.stateId) {
            throw CommandSyntaxException(Component.translatable("commands.setblock.failed"))
        }
        if (mode == Mode.DESTROY) editor.set(x, y, z, Blocks.AIR.stateId)
        if (!editor.set(x, y, z, block.stateId) && mode != Mode.DESTROY) {
            throw CommandSyntaxException(Component.translatable("commands.setblock.failed"))
        }
        editor.flush()

        context.success(
            Component.translatable(
                "commands.setblock.success",
                Component.text(x.toString()),
                Component.text(y.toString()),
                Component.text(z.toString()),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }
}
