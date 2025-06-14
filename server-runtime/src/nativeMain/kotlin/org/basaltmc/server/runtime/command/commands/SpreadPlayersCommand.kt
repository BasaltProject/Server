package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.Vec2Argument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.text.Component
import kotlin.math.sqrt
import kotlin.random.Random

/**
 * parity: `SpreadPlayersCommand`, including its relaxation loop: positions start random inside the
 * circle and are then pushed apart until every pair is at least [spreadDistance] apart or the
 * attempts run out, which is exactly why vanilla can fail with "too many players".
 *
 * `under <maxHeight>` is accepted and honoured as a ceiling on the landing height; teams are not,
 * because spreading by team needs the team of an entity that is not a player.
 */
public class SpreadPlayersCommand : Command(name = "spreadplayers", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("center", Vec2Argument) {
            argument("spreadDistance", FloatArgument(min = 0f)) {
                argument("maxRange", FloatArgument(min = 1f)) {
                    argument("respectTeams", BoolArgument) {
                        argument("targets", PlayersArgument(single = false)) {
                            executes { context -> spread(context, null) }
                        }
                    }
                    literal("under") {
                        argument(
                            "maxHeight",
                            org.basaltmc.server.runtime.command
                                .IntegerArgument(),
                        ) {
                            argument("respectTeams", BoolArgument) {
                                argument("targets", PlayersArgument(single = false)) {
                                    executes { context -> spread(context, context.get("maxHeight")) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun spread(
        context: CommandContext,
        maxHeight: Int?,
    ): Int {
        val world = context.world()
        val targets = context.get<List<PlayerSession>>("targets")
        val spreadDistance = context.get<Double>("spreadDistance")
        val maxRange = context.get<Double>("maxRange")
        val (centerX, _, centerZ) = context.get<Coordinates>("center").resolve(context.source.position)

        if (spreadDistance > maxRange) {
            throw CommandSyntaxException(
                Component.text("The spread distance must not be larger than the maximum range."),
            )
        }
        val random = Random.Default
        val positions =
            MutableList(targets.size) {
                (centerX - maxRange + random.nextDouble() * maxRange * 2) to
                    (centerZ - maxRange + random.nextDouble() * maxRange * 2)
            }

        var attempts = 0
        while (attempts < MAX_ATTEMPTS && !spreadApart(positions, spreadDistance)) attempts++
        if (attempts >= MAX_ATTEMPTS) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.spreadplayers.failed.entities",
                    Component.text(targets.size.toString()),
                    Component.text(centerX.toString()),
                    Component.text(centerZ.toString()),
                    Component.text(spreadDistance.toString()),
                ),
            )
        }

        for ((index, target) in targets.withIndex()) {
            val (x, z) = positions[index]
            val column = world.state.chunkIfLoaded(x.toInt() shr 4, z.toInt() shr 4)
            val surface =
                column?.heightAt(x.toInt() and 15, z.toInt() and 15, Chunk.HeightmapType.MOTION_BLOCKING)?.toDouble()
                    ?: target.y
            val y = maxHeight?.let { minOf(surface, it.toDouble()) } ?: surface
            target.connection.teleportTo(x, y, z, target.yaw, target.pitch)
        }

        context.success(
            Component.translatable(
                "commands.spreadplayers.success.entities",
                Component.text(targets.size.toString()),
                Component.text(centerX.toString()),
                Component.text(centerZ.toString()),
                Component.text(averageDistance(positions)),
            ),
            broadcastToAdmins = true,
        )
        return targets.size
    }

    /** parity: the mean distance between every pair, which is the number vanilla reports. */
    private fun averageDistance(positions: List<Pair<Double, Double>>): String {
        if (positions.size < 2) return "0.00"
        var total = 0.0
        var pairs = 0
        for (i in positions.indices) {
            for (j in i + 1 until positions.size) {
                val dx = positions[i].first - positions[j].first
                val dz = positions[i].second - positions[j].second
                total += kotlin.math.sqrt(dx * dx + dz * dz)
                pairs++
            }
        }
        val scaled = kotlin.math.round(total / pairs * 100.0).toLong()
        return "${scaled / 100}.${(scaled % 100).toString().padStart(2, '0')}"
    }

    /** One relaxation pass. Returns true once nothing had to move, which is when the spread is done. */
    private fun spreadApart(
        positions: MutableList<Pair<Double, Double>>,
        spreadDistance: Double,
    ): Boolean {
        var settled = true
        for (i in positions.indices) {
            var pushX = 0.0
            var pushZ = 0.0
            var pushes = 0
            for (j in positions.indices) {
                if (i == j) continue
                val dx = positions[i].first - positions[j].first
                val dz = positions[i].second - positions[j].second
                val distance = sqrt(dx * dx + dz * dz)
                if (distance >= spreadDistance) continue
                settled = false
                pushes++
                if (distance == 0.0) {
                    pushX += 1.0
                } else {
                    pushX += dx / distance
                    pushZ += dz / distance
                }
            }
            if (pushes == 0) continue
            positions[i] =
                (positions[i].first + pushX / pushes * spreadDistance) to
                (positions[i].second + pushZ / pushes * spreadDistance)
        }
        return settled
    }

    private companion object {
        /** parity: vanilla gives up after this many relaxation passes and reports failure. */
        const val MAX_ATTEMPTS = 10000
    }
}
