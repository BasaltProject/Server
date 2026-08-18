package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component

/** parity: `KillCommand`. Vanilla's `Entity.kill` is unblockable damage, so creative mode does not save you. */
public class KillCommand : Command(name = "kill", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> kill(context, listOf(CommandTarget.Player(context.requireEntity()))) }
        root.argument("targets", EntitiesArgument(single = false)) {
            executes { context -> kill(context, context.get("targets")) }
        }
    }

    private fun kill(
        context: CommandContext,
        targets: List<CommandTarget>,
    ): Int {
        for (target in targets) {
            when (target) {
                is CommandTarget.Player -> target.session.connection.killPlayer(DEATH_GENERIC_KILL)
                // A summoned entity has no death animation to play, so it simply stops existing.
                is CommandTarget.Entity -> {
                    // parity: `/kill` kills, so what it kills drops what it would have dropped.
                    target.entity.killed = true
                    target.entity.removed = true
                }
                // parity: an item entity is discarded rather than killed: it leaves nothing behind.
                is CommandTarget.Item ->
                    context.environment.worlds[target.worldName]
                        ?.state
                        ?.removeItem(target.entityId)
            }
        }
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
