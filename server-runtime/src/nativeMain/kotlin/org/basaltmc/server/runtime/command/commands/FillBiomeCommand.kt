package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * parity: `FillBiomeCommand`, which works in quarts: biomes are stored one per 4x4x4 block, so a
 * region smaller than that still changes the whole quart it falls in, exactly as vanilla does.
 *
 * The change reaches clients through `chunks_biomes` rather than a chunk resend, which is what
 * vanilla sends here too: only the biome containers are replaced, so the column does not blink.
 */
public class FillBiomeCommand : Command(name = "fillbiome", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("from", BlockPosArgument) {
            argument("to", BlockPosArgument) {
                argument("biome", RegistryArgument.resource("minecraft:worldgen/biome")) {
                    suggestsFromServer { context -> context.matchingIds(Biomes.REGISTRY.identifiers()) }
                    executes { context -> fill(context, null) }
                    literal("replace") {
                        argument("filter", RegistryArgument.orTag("minecraft:worldgen/biome")) {
                            suggestsFromServer { context -> context.matchingIds(Biomes.REGISTRY.identifiers()) }
                            executes { context -> fill(context, context.get("filter")) }
                        }
                    }
                }
            }
        }
    }

    private fun fill(
        context: CommandContext,
        filter: Identifier?,
    ): Int {
        val world = context.world()
        val origin = context.source.position
        val (fromX, fromY, fromZ) = context.get<BlockCoordinates>("from").resolve(origin)
        val (toX, toY, toZ) = context.get<BlockCoordinates>("to").resolve(origin)
        val biomeId = context.get<Identifier>("biome")
        val biome =
            Biomes.REGISTRY.idOf(biomeId)
                ?: throw CommandSyntaxException(
                    Component.translatable(
                        "argument.resource.not_found",
                        Component.text(biomeId.toString()),
                        Component.text("minecraft:worldgen/biome"),
                    ),
                )
        val filterId = filter?.let { Biomes.REGISTRY.idOf(it) }
        if (filter != null && filterId == null) {
            throw CommandSyntaxException(
                Component.translatable(
                    "argument.resource.not_found",
                    Component.text(filter.toString()),
                    Component.text("minecraft:worldgen/biome"),
                ),
            )
        }

        val bounds = world.declaration.dimension.chunkHeight
        val minQuartX = minOf(fromX, toX) shr 2
        val maxQuartX = maxOf(fromX, toX) shr 2
        val minQuartZ = minOf(fromZ, toZ) shr 2
        val maxQuartZ = maxOf(fromZ, toZ) shr 2
        val minQuartY = (minOf(fromY, toY).coerceAtLeast(bounds.minY) - bounds.minY) shr 2
        val maxQuartY = (maxOf(fromY, toY).coerceAtMost(bounds.maxY) - bounds.minY) shr 2

        val volume = (maxQuartX - minQuartX + 1).toLong() * (maxQuartY - minQuartY + 1) * (maxQuartZ - minQuartZ + 1)
        if (volume > MAX_QUARTS) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.fillbiome.toobig",
                    Component.text(MAX_QUARTS.toString()),
                    Component.text(volume.toString()),
                ),
            )
        }

        var changed = 0
        for (quartX in minQuartX..maxQuartX) {
            for (quartZ in minQuartZ..maxQuartZ) {
                val chunk =
                    world.state.chunkIfLoaded(quartX shr 2, quartZ shr 2)
                        ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
                for (quartY in minQuartY..maxQuartY) {
                    if (filterId != null && chunk.getBiomeId(quartX and 3, quartY, quartZ and 3) != filterId) continue
                    chunk.setBiomeId(quartX and 3, quartY, quartZ and 3, biome)
                    changed++
                }
            }
        }

        context.environment.syncBiomes(
            world,
            minOf(fromX, toX),
            minOf(fromZ, toZ),
            maxOf(fromX, toX),
            maxOf(fromZ, toZ),
        )

        context.success(
            Component.translatable(
                // parity: the string takes the count and both corners, seven arguments in total.
                "commands.fillbiome.success.count",
                Component.text(changed.toString()),
                Component.text(minOf(fromX, toX).toString()),
                Component.text(minOf(fromY, toY).toString()),
                Component.text(minOf(fromZ, toZ).toString()),
                Component.text(maxOf(fromX, toX).toString()),
                Component.text(maxOf(fromY, toY).toString()),
                Component.text(maxOf(fromZ, toZ).toString()),
            ),
            broadcastToAdmins = true,
        )
        return changed
    }

    private companion object {
        /** parity: vanilla's own cap, in quarts rather than blocks. */
        const val MAX_QUARTS = 32768L
    }
}
