package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.command.PlayersArgument
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component

/** parity: `KillCommand`. Vanilla's `Entity.kill` is unblockable damage, so creative mode does not save you. */
public class KillCommand : Command(name = "kill", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> kill(context, listOf(context.requireEntity())) }
        root.argument("targets", PlayersArgument(single = false)) {
            executes { context -> kill(context, context.get("targets")) }
        }
    }

    private fun kill(
        context: CommandContext,
        targets: List<PlayerSession>,
    ): Int {
        for (target in targets) target.connection.killPlayer(DEATH_GENERIC_KILL)
        val message =
            if (targets.size == 1) {
                Component.translatable("commands.kill.success.single", Component.text(targets.single().name))
            } else {
                Component.translatable("commands.kill.success.multiple", Component.text(targets.size.toString()))
            }
        context.success(message, broadcastToAdmins = true)
        return targets.size
    }

    private companion object {
        /** parity: `DamageTypes.GENERIC_KILL`'s message id, which the client renders as "<name> died". */
        const val DEATH_GENERIC_KILL = "genericKill"
    }
}
