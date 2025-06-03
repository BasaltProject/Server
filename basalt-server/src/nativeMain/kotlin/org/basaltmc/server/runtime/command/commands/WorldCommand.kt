package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.WorldArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * `/world`: which worlds exist, and moving between them.
 *
 * No vanilla counterpart, because vanilla has no worlds to move between: it has three dimensions of
 * one world, reached by building a portal. `/execute in <dimension>` is the closest thing and it is
 * a different idea. So this is Basalt's, like `import-players`.
 *
 * Travel itself is not reimplemented here: it is the same `switchWorld` that `/tp ... <world>`
 * already uses, which is what keeps one way of changing world rather than two that can drift.
 */
public class WorldCommand : Command(name = "world", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> list(context) }

        root.literal("list") {
            executes { context -> list(context) }
        }

        root.literal("info") {
            argument("world", WorldArgument) {
                executes { context -> info(context, context.get("world")) }
            }
        }

        // `/world <name>` sends whoever ran it, which is the form anyone actually types.
        root.argument("world", WorldArgument) {
            executes { context ->
                send(context, listOf(context.requireEntity()), context.get("world"))
            }
            argument("targets", PlayersArgument(single = false)) {
                executes { context ->
                    send(context, context.get("targets"), context.get("world"))
                }
            }
        }
    }

    private fun list(context: CommandContext): Int {
        val worlds =
            context.environment.worlds.values
                .sortedBy { it.name }
        val here = context.source.player?.worldName
        context.source.sendSystemMessage(
            Component.text("${worlds.size} world(s) loaded:", TextColor.GRAY),
        )
        for (world in worlds) {
            val players =
                context.environment.players
                    .snapshot()
                    .count { it.worldName == world.name }
            val marker = if (world.name == here) " <-- you are here" else ""
            context.source.sendSystemMessage(
                Component
                    .text("  ${world.name}", TextColor.WHITE)
                    .append(
                        Component.text(
                            "  ${world.declaration.type.name.lowercase()}  $players player(s)$marker",
                            TextColor.GRAY,
                        ),
                    ),
            )
        }
        return worlds.size
    }

    private fun info(
        context: CommandContext,
        world: LoadedWorld,
    ): Int {
        val players =
            context.environment.players
                .snapshot()
                .count { it.worldName == world.name }
        for (line in listOf(
            "world: ${world.name}",
            "type: ${world.declaration.type.name.lowercase()}",
            "dimension: ${world.declaration.dimension}",
            "seed: ${world.metadata.seed}",
            "spawn: ${world.spawn.first}, ${world.spawn.second}, ${world.spawn.third}",
            "chunks loaded: ${world.state.loadedChunkCount}",
            "players: $players",
        )) {
            context.source.sendSystemMessage(Component.text(line, TextColor.GRAY))
        }
        return 1
    }

    /**
     * Puts [targets] in [world], at its spawn.
     *
     * At the spawn rather than at the same coordinates: the two worlds share nothing, and the same
     * x/z in another world is as likely to be inside a mountain as anywhere else. `/tp` is still
     * there for landing somewhere exact.
     */
    private fun send(
        context: CommandContext,
        targets: List<PlayerSession>,
        world: LoadedWorld,
    ): Int {
        val (x, y, z) = world.spawn
        var moved = 0
        for (target in targets) {
            if (target.worldName == world.name) {
                if (targets.size == 1) {
                    throw CommandSyntaxException(Component.text("${target.name} is already in ${world.name}."))
                }
                continue
            }
            if (target.connection.switchWorld(world, x, y, z)) moved++
        }
        if (moved == 0) throw CommandSyntaxException(Component.text("Nobody moved."))

        val who = if (moved == 1) targets.first { it.worldName == world.name }.name else "$moved players"
        context.success(Component.text("Moved $who to ${world.name}."), broadcastToAdmins = true)
        return moved
    }
}
