package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.ParsedParticle
import org.basaltmc.server.runtime.command.ParticleArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.text.Component

/**
 * parity: `ParticleCommand`.
 *
 * The options a type carries are parsed and written by [ParticleArgument]: they go straight after
 * the type id with no length of their own, so a missing or wrongly shaped block desynchronises the
 * client on the very next packet rather than drawing the wrong particle.
 */
public class ParticleCommand : Command(name = "particle", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("name", ParticleArgument) {
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
        val parsed = context.get<ParsedParticle>("name")
        val type = parsed.type
        val id = type.id
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
                options = parsed.options,
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
