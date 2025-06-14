package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.RotationArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.storage.StoredSpawnPoint
import org.basaltmc.wire.text.Component

/**
 * parity: `SetSpawnCommand`.
 *
 * The point is stored per player in `players.db`, so it outlives both the session and the world it
 * names: a world that is no longer loaded when they die sends them to the default world's spawn,
 * the same fallback a stored position gets.
 */
public class SpawnPointCommand : Command(name = "spawnpoint", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> setSpawn(context, listOf(context.requirePlayer()), null, 0f) }
        root.argument("targets", PlayersArgument(single = false)) {
            executes { context -> setSpawn(context, context.get("targets"), null, 0f) }
            argument("pos", BlockPosArgument) {
                executes { context -> setSpawn(context, context.get("targets"), context.get("pos"), 0f) }
                argument("rotation", RotationArgument) {
                    executes { context ->
                        setSpawn(
                            context,
                            context.get("targets"),
                            context.get("pos"),
                            context.get<Pair<Float, Float>>("rotation").first,
                        )
                    }
                }
            }
        }
    }

    private fun setSpawn(
        context: CommandContext,
        targets: List<PlayerSession>,
        coordinates: BlockCoordinates?,
        angle: Float,
    ): Int {
        val access =
            context.environment.access
                ?: throw CommandSyntaxException(
                    Component.text("This server has no player database to remember a spawn point in."),
                )
        val world = context.world()
        for (target in targets) {
            val (x, y, z) =
                coordinates?.resolve(Triple(target.x, target.y, target.z))
                    ?: Triple(
                        kotlin.math.floor(target.x).toInt(),
                        kotlin.math.floor(target.y).toInt(),
                        kotlin.math.floor(target.z).toInt(),
                    )
            access.setSpawnPoint(
                StoredSpawnPoint(
                    uuid = target.uuid.toString(),
                    world = target.worldName.ifEmpty { world.name },
                    x = x + 0.5,
                    y = y.toDouble(),
                    z = z + 0.5,
                    yaw = angle,
                ),
            )
        }
        val (x, y, z) =
            coordinates?.resolve(context.source.position)
                ?: Triple(
                    kotlin.math.floor(context.source.position.first).toInt(),
                    kotlin.math.floor(context.source.position.second).toInt(),
                    kotlin.math.floor(context.source.position.third).toInt(),
                )
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.spawnpoint.success.single",
                    Component.text(x.toString()),
                    Component.text(y.toString()),
                    Component.text(z.toString()),
                    Component.text(angle.toString()),
                    Component.text(world.name),
                    Component.text(targets.single().name),
                )
            } else {
                Component.translatable(
                    "commands.spawnpoint.success.multiple",
                    Component.text(x.toString()),
                    Component.text(y.toString()),
                    Component.text(z.toString()),
                    Component.text(angle.toString()),
                    Component.text(world.name),
                    Component.text(targets.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return targets.size
    }
}
