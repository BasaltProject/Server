package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.Difficulty
import org.basaltmc.wire.protocol.play.ChangeDifficultyPacket
import org.basaltmc.wire.text.Component

/**
 * parity: `DifficultyCommand`, including its refusal when the world is already on that difficulty.
 *
 * The value is this world's, not the server's, because that is where vanilla keeps it, and it now
 * outlives a restart in `level.nbt` rather than in `worlds.toml`, which the operator owns.
 */
public class DifficultyCommand : Command(name = "difficulty", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> query(context) }
        for (difficulty in Difficulty.entries) {
            root.literal(difficulty.name.lowercase()) { executes { context -> set(context, difficulty) } }
        }
    }

    private fun query(context: CommandContext): Int {
        val world = context.world()
        context.success(
            Component.translatable(
                "commands.difficulty.query",
                Component.translatable("options.difficulty.${world.difficulty.lowercase()}"),
            ),
        )
        return Difficulty.entries.firstOrNull { it.name.equals(world.difficulty, ignoreCase = true) }?.ordinal ?: 0
    }

    private fun set(
        context: CommandContext,
        difficulty: Difficulty,
    ): Int {
        val world = context.world()
        val name = difficulty.name.lowercase()
        if (world.difficulty.equals(name, ignoreCase = true)) {
            throw CommandSyntaxException(
                Component.translatable("commands.difficulty.failure", Component.text(name)),
            )
        }
        world.difficulty = name
        val packet = ChangeDifficultyPacket(difficulty.ordinal, world.difficultyLocked)
        for (player in context.environment.playersIn(world)) {
            player.connection.sendPlayPacket(ChangeDifficultyPacket.ID) { packet.write(it) }
        }
        context.success(
            Component.translatable(
                "commands.difficulty.success",
                Component.translatable("options.difficulty.$name"),
            ),
            broadcastToAdmins = true,
        )
        return difficulty.ordinal
    }
}
