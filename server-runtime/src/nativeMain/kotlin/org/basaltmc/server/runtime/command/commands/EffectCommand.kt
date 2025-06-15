package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * parity: `EffectCommand`.
 *
 * Only players carry effects here: nothing else in this build has the health or the hunger the
 * effects act on, so a selector that matches something else is refused by name.
 */
public class EffectCommand : Command(name = "effect", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("clear") {
            executes { context -> clear(context, self(context), null) }
            argument("targets", EntitiesArgument()) {
                executes { context -> clear(context, players(context), null) }
                argument("effect", RegistryArgument.resource("minecraft:mob_effect")) {
                    executes { context -> clear(context, players(context), context.get("effect")) }
                }
            }
        }
        root.literal("give") {
            argument("targets", EntitiesArgument()) {
                argument("effect", RegistryArgument.resource("minecraft:mob_effect")) {
                    executes { context -> give(context, DEFAULT_SECONDS * TICKS_PER_SECOND, 0, true) }
                    literal("infinite") {
                        executes { context -> give(context, INFINITE, 0, true) }
                        argument("amplifier", IntegerArgument(min = 0, max = MAX_AMPLIFIER)) {
                            executes { context -> give(context, INFINITE, context.get("amplifier"), true) }
                            argument("hideParticles", BoolArgument) {
                                executes { context ->
                                    give(
                                        context,
                                        INFINITE,
                                        context.get("amplifier"),
                                        !context.get<Boolean>("hideParticles"),
                                    )
                                }
                            }
                        }
                    }
                    argument("seconds", IntegerArgument(min = 0, max = MAX_SECONDS)) {
                        executes { context -> give(context, context.get<Int>("seconds") * TICKS_PER_SECOND, 0, true) }
                        argument("amplifier", IntegerArgument(min = 0, max = MAX_AMPLIFIER)) {
                            executes { context ->
                                give(
                                    context,
                                    context.get<Int>("seconds") * TICKS_PER_SECOND,
                                    context.get("amplifier"),
                                    true,
                                )
                            }
                            argument("hideParticles", BoolArgument) {
                                executes { context ->
                                    give(
                                        context,
                                        context.get<Int>("seconds") * TICKS_PER_SECOND,
                                        context.get("amplifier"),
                                        !context.get<Boolean>("hideParticles"),
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun give(
        context: CommandContext,
        durationTicks: Int,
        amplifier: Int,
        visible: Boolean,
    ): Int {
        val effect = context.get<Identifier>("effect")
        if (MobEffects.protocolIdOf(effect) == null) {
            throw CommandSyntaxException(
                Component.translatable("effect.effectNotFound", Component.text(effect.toString())),
            )
        }
        val targets = players(context)
        for (player in targets) {
            // parity: the two instantaneous effects act once and are never carried.
            if (effect == MobEffects.INSTANT_HEALTH) {
                player.connection.heal(INSTANT_AMOUNT * (1 shl amplifier))
                continue
            }
            if (effect == MobEffects.INSTANT_DAMAGE) {
                player.connection.hurt(INSTANT_AMOUNT * (1 shl amplifier), "magic")
                continue
            }
            val active = ActiveEffect(effect, amplifier, durationTicks, visible = visible, showIcon = visible)
            player.effects[effect] = active
            player.connection.sendEffect(active)
        }
        context.success(
            Component.translatable(
                "commands.effect.give.success.single",
                Component.text(effect.path),
                Component.text(targets.singleOrNull()?.name ?: targets.size.toString()),
            ),
            broadcastToAdmins = true,
        )
        return targets.size
    }

    private fun clear(
        context: CommandContext,
        targets: List<PlayerSession>,
        effect: Identifier?,
    ): Int {
        var cleared = 0
        for (player in targets) {
            val removed =
                if (effect ==
                    null
                ) {
                    player.effects.keys.toList()
                } else {
                    listOfNotNull(effect.takeIf { it in player.effects })
                }
            for (id in removed) {
                player.effects.remove(id)
                player.connection.sendEffectRemoved(id)
                cleared++
            }
        }
        context.success(
            Component.translatable(
                "commands.effect.clear.everything.success.single",
                Component.text(targets.singleOrNull()?.name ?: targets.size.toString()),
            ),
            broadcastToAdmins = true,
        )
        return cleared
    }

    private fun self(context: CommandContext): List<PlayerSession> = listOfNotNull(context.source.player)

    private fun players(context: CommandContext): List<PlayerSession> =
        context.get<List<CommandTarget>>("targets").map { target ->
            (target as? CommandTarget.Player)?.session
                ?: throw CommandSyntaxException(
                    Component.text("Only a player carries effects in this build; ${target.name} does not."),
                )
        }

    private companion object {
        const val TICKS_PER_SECOND = 20
        const val DEFAULT_SECONDS = 30
        const val INFINITE = -1
        const val MAX_SECONDS = 1_000_000
        const val MAX_AMPLIFIER = 255

        /** parity: `HealOrHarmMobEffect`, three hearts' worth doubled by each level. */
        const val INSTANT_AMOUNT = 6f
    }
}
