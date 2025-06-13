package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.ParticleTypes
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.text.Component

/**
 * parity: `ParticleCommand`, for every particle that carries no options.
 *
 * A particle with options (`block`, `dust`, `item`, `trail`, and the rest of the twenty-two) needs
 * its option block written after the type id, and a packet missing it desynchronizes the client on
 * the very next frame, so those are refused by name instead.
 */
public class ParticleCommand : Command(name = "particle", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("name", ResourceLocationArgument) {
            executes { context -> particle(context, null, null, 1, force = false, viewers = null) }
            argument("pos", Vec3Argument) {
                executes { context -> particle(context, context.get("pos"), null, 1, force = false, viewers = null) }
                argument("delta", Vec3Argument) {
                    argument("speed", FloatArgument(min = 0f)) {
                        argument("count", IntegerArgument(min = 0)) {
                            executes { context ->
                                particle(
                                    context,
                                    context.get("pos"),
                                    context.get("delta"),
                                    context.get("count"),
                                    force = false,
                                    viewers = null,
                                )
                            }
                            for (mode in listOf("force", "normal")) {
                                literal(mode) {
                                    executes { context ->
                                        particle(
                                            context,
                                            context.get("pos"),
                                            context.get("delta"),
                                            context.get("count"),
                                            force = mode == "force",
                                            viewers = null,
                                        )
                                    }
                                    argument("viewers", PlayersArgument(single = false)) {
                                        executes { context ->
                                            particle(
                                                context,
                                                context.get("pos"),
                                                context.get("delta"),
                                                context.get("count"),
                                                force = mode == "force",
                                                viewers = context.get("viewers"),
                                            )
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun particle(
        context: CommandContext,
        position: Coordinates?,
        delta: Coordinates?,
        count: Int,
        force: Boolean,
        viewers: List<PlayerSession>?,
    ): Int {
        val id = context.get<Identifier>("name")
        val type =
            ParticleTypes[id]
                ?: throw CommandSyntaxException(
                    Component.translatable("particle.notFound", Component.text(id.toString())),
                )
        if (type.needsOptions) {
            throw CommandSyntaxException(
                Component.text(
                    "'$id' carries extra data (a block, an item or colours) that this build cannot write yet, " +
                        "so it would arrive as a different particle or break the connection.",
                ),
            )
        }
        val world = context.world()
        val (x, y, z) = position?.resolve(context.source.position) ?: context.source.position
        val (dx, dy, dz) = delta?.resolve(Triple(0.0, 0.0, 0.0)) ?: Triple(0.0, 0.0, 0.0)
        val speed = if (context.has("speed")) context.get<Float>("speed") else 0f

        val packet =
            LevelParticlesPacket(
                particleTypeId = type.protocolId,
                x = x,
                y = y,
                z = z,
                offsetX = dx.toFloat(),
                offsetY = dy.toFloat(),
                offsetZ = dz.toFloat(),
                maxSpeed = speed,
                count = count,
                overrideLimiter = force,
            )
        val audience = viewers ?: context.environment.playersIn(world)
        if (audience.isEmpty()) {
            throw CommandSyntaxException(
                Component.translatable("commands.particle.failed"),
            )
        }
        for (player in audience) {
            player.connection.sendPlayPacket(LevelParticlesPacket.ID) { packet.write(it) }
        }
        context.success(
            Component.translatable("commands.particle.success", Component.text(id.toString())),
        )
        return audience.size
    }
}
