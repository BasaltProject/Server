package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.SuggestionProviders
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.protocol.play.StopSoundPacket
import org.basaltmc.wire.text.Component
import kotlin.math.sqrt
import kotlin.random.Random

/**
 * parity: `PlaySoundCommand`, including the distance rule: a player further away than the sound
 * carries hears it moved towards them at [MIN_HEARABLE_VOLUME] instead of not at all.
 */
public class PlaySoundCommand : Command(name = "playsound", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("sound", ResourceLocationArgument) {
            suggestsFromClient(SuggestionProviders.AVAILABLE_SOUNDS)
            for (source in SoundSource.entries) {
                literal(source.commandName) {
                    argument("targets", PlayersArgument(single = false)) {
                        executes { context -> play(context, source, null, 1f, 1f, 0f) }
                        argument("pos", Vec3Argument) {
                            executes { context -> play(context, source, context.get("pos"), 1f, 1f, 0f) }
                            argument("volume", FloatArgument(min = 0f)) {
                                executes { context ->
                                    play(context, source, context.get("pos"), context.get("volume"), 1f, 0f)
                                }
                                argument("pitch", FloatArgument(min = 0f, max = 2f)) {
                                    executes { context ->
                                        play(
                                            context,
                                            source,
                                            context.get("pos"),
                                            context.get("volume"),
                                            context.get("pitch"),
                                            0f,
                                        )
                                    }
                                    argument("minVolume", FloatArgument(min = 0f, max = 1f)) {
                                        executes { context ->
                                            play(
                                                context,
                                                source,
                                                context.get("pos"),
                                                context.get("volume"),
                                                context.get("pitch"),
                                                context.get("minVolume"),
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

    private fun play(
        context: CommandContext,
        source: SoundSource,
        coordinates: Coordinates?,
        volume: Float,
        pitch: Float,
        minVolume: Float,
    ): Int {
        val sound = context.get<Identifier>("sound")
        val targets = context.get<List<PlayerSession>>("targets")
        val origin = coordinates?.resolve(context.source.position) ?: context.source.position
        val (x, y, z) = origin
        // parity: the sound carries 16 blocks per unit of volume, and beyond that vanilla moves it
        // to just within earshot rather than dropping it, but only if a minimum volume was given.
        val range = volume.toDouble() * AUDIBLE_BLOCKS_PER_VOLUME
        var played = 0
        for (target in targets) {
            val dx = x - target.x
            val dy = y - target.y
            val dz = z - target.z
            val distance = sqrt(dx * dx + dy * dy + dz * dz)
            var soundX = x
            var soundY = y
            var soundZ = z
            var effectiveVolume = volume
            if (distance > range) {
                if (minVolume <= 0f) continue
                val scale = range / distance
                soundX = target.x + dx * scale
                soundY = target.y + dy * scale
                soundZ = target.z + dz * scale
                effectiveVolume = minVolume
            }
            val packet =
                SoundPacket(
                    sound.toString(),
                    source,
                    soundX,
                    soundY,
                    soundZ,
                    effectiveVolume,
                    pitch,
                    Random.nextLong(),
                )
            target.connection.sendPlayPacket(SoundPacket.ID) { packet.write(it) }
            played++
        }
        if (played == 0) throw CommandSyntaxException(Component.translatable("commands.playsound.failed"))

        context.success(
            if (played == 1) {
                Component.translatable(
                    "commands.playsound.success.single",
                    Component.text(sound.toString()),
                    Component.text(targets.first().name),
                )
            } else {
                Component.translatable(
                    "commands.playsound.success.multiple",
                    Component.text(sound.toString()),
                    Component.text(played.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return played
    }

    private companion object {
        /** parity: how far a sound of volume 1 carries before vanilla considers it out of earshot. */
        const val AUDIBLE_BLOCKS_PER_VOLUME = 16.0
    }
}

/** parity: `StopSoundCommand`, whose four success keys say exactly which half was given. */
public class StopSoundCommand : Command(name = "stopsound", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            executes { context -> stop(context, null, null) }
            literal("*") {
                executes { context -> stop(context, null, null) }
                argument("sound", ResourceLocationArgument) {
                    suggestsFromClient(SuggestionProviders.AVAILABLE_SOUNDS)
                    executes { context -> stop(context, null, context.get("sound")) }
                }
            }
            for (source in SoundSource.entries) {
                literal(source.commandName) {
                    executes { context -> stop(context, source, null) }
                    argument("sound", ResourceLocationArgument) {
                        suggestsFromClient(SuggestionProviders.AVAILABLE_SOUNDS)
                        executes { context -> stop(context, source, context.get("sound")) }
                    }
                }
            }
        }
    }

    private fun stop(
        context: CommandContext,
        source: SoundSource?,
        sound: Identifier?,
    ): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        val packet = StopSoundPacket(sound?.toString(), source)
        for (target in targets) {
            target.connection.sendPlayPacket(StopSoundPacket.ID) { packet.write(it) }
        }
        val message =
            when {
                source != null && sound != null ->
                    Component.translatable(
                        "commands.stopsound.success.source.sound",
                        Component.text(sound.toString()),
                        Component.text(source.commandName),
                    )
                source != null ->
                    Component.translatable(
                        "commands.stopsound.success.source.any",
                        Component.text(source.commandName),
                    )
                sound != null ->
                    Component.translatable(
                        "commands.stopsound.success.sourceless.sound",
                        Component.text(sound.toString()),
                    )
                else -> Component.translatable("commands.stopsound.success.sourceless.any")
            }
        context.success(message, broadcastToAdmins = true)
        return targets.size
    }
}
