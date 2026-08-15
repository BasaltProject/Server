package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RotationArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.GlobalPos
import org.basaltmc.wire.protocol.play.SpawnPositionPacket
import org.basaltmc.wire.text.Component

/** parity: `SetWorldSpawnCommand`. The new spawn goes into `level.nbt`, so it outlives a restart. */
public class SetWorldSpawnCommand : Command(name = "setworldspawn", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> setSpawn(context, null, 0f) }
        root.argument("pos", BlockPosArgument) {
            executes { context -> setSpawn(context, context.get("pos"), 0f) }
            argument("rotation", RotationArgument) {
                executes { context ->
                    setSpawn(context, context.get("pos"), context.get<Pair<Float, Float>>("rotation").first)
                }
            }
        }
    }

    private fun setSpawn(
        context: CommandContext,
        coordinates: BlockCoordinates?,
        angle: Float,
    ): Int {
        val world = context.world()
        val (x, y, z) =
            coordinates?.resolve(context.source.position)
                ?: context.source.position.let { (px, py, pz) ->
                    Triple(kotlin.math.floor(px).toInt(), kotlin.math.floor(py).toInt(), kotlin.math.floor(pz).toInt())
                }
        world.setSpawn(x, y, z, angle)

        val packet = SpawnPositionPacket(GlobalPos(world.levelKey, BlockPos(x, y, z)), yaw = angle, pitch = 0f)
        for (player in context.environment.playersIn(world)) {
            player.connection.sendPlayPacket(SpawnPositionPacket.ID) { packet.write(it) }
        }
        context.success(
            Component.translatable(
                "commands.setworldspawn.success",
                Component.text(x.toString()),
                Component.text(y.toString()),
                Component.text(z.toString()),
                Component.text(angle.toString()),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }
}
