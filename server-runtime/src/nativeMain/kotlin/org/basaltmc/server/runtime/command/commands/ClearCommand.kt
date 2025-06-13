package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.ItemPredicateArgument
import org.basaltmc.server.runtime.command.ItemPredicateInput
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.storage.StoredInventory
import org.basaltmc.wire.text.Component

/**
 * parity: `ClearInventoryCommands`, including `maxCount` 0, which counts without removing anything
 * and reports through the `commands.clear.test.*` keys instead.
 */
public class ClearCommand : Command(name = "clear", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> clear(context, listOf(context.requirePlayer()), null, -1) }
        root.argument("targets", PlayersArgument(single = false)) {
            executes { context -> clear(context, context.get("targets"), null, -1) }
            argument("item", ItemPredicateArgument) {
                executes { context -> clear(context, context.get("targets"), context.get("item"), -1) }
                argument("maxCount", IntegerArgument(min = 0)) {
                    executes { context ->
                        clear(context, context.get("targets"), context.get("item"), context.get("maxCount"))
                    }
                }
            }
        }
    }

    private fun clear(
        context: CommandContext,
        targets: List<PlayerSession>,
        predicate: ItemPredicateInput?,
        maxCount: Int,
    ): Int {
        var cleared = 0
        for (target in targets) {
            var remaining = if (maxCount < 0) Int.MAX_VALUE else maxCount
            for (slot in 0 until StoredInventory.PLAYER_SLOTS) {
                val stack = target.inventory[slot] ?: continue
                if (predicate != null && !predicate.matches(stack.id)) continue
                if (remaining == 0) {
                    // maxCount 0 is vanilla's "count only" form: nothing is taken, but everything
                    // that would have been is still counted.
                    cleared += stack.count
                    continue
                }
                val taken = minOf(stack.count, remaining)
                cleared += taken
                remaining -= taken
                target.inventory[slot] =
                    if (taken >= stack.count) null else stack.copy(count = stack.count - taken)
            }
            if (maxCount != 0) target.connection.sendInventoryAll()
        }

        if (cleared == 0) {
            throw CommandSyntaxException(
                if (targets.size == 1) {
                    Component.translatable("clear.failed.single", Component.text(targets.single().name))
                } else {
                    Component.translatable("clear.failed.multiple", Component.text(targets.size.toString()))
                },
            )
        }
        val kind = if (maxCount == 0) "test" else "success"
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.clear.$kind.single",
                    Component.text(cleared.toString()),
                    Component.text(targets.single().name),
                )
            } else {
                Component.translatable(
                    "commands.clear.$kind.multiple",
                    Component.text(cleared.toString()),
                    Component.text(targets.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return cleared
    }
}
