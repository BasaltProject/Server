package org.basaltmc.server.command

import org.basaltmc.protocol.play.Gamemode
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component
import kotlin.math.abs
import kotlin.math.roundToLong

/**
 * The commands Basalt currently understands, registered onto [dispatcher].
 *
 * Every one of them is a reimplementation of the class of the same name under
 * `net.minecraft.server.commands`: the same tree shape, the same permission level, and the same
 * translation keys for its feedback, because the client resolves those keys itself and a different
 * key is a visible divergence. What is *not* here is most of vanilla's command set (the world,
 * scoreboard, data and datapack families) plus the selector filter syntax: both are declared gaps,
 * not silent ones.
 */
public fun registerVanillaCommands(dispatcher: CommandDispatcher) {
    val feedback = CommandFeedback(dispatcher.environment)
    registerHelp(dispatcher)
    registerList(dispatcher, feedback)
    registerSeed(dispatcher, feedback)
    registerSay(dispatcher)
    registerMe(dispatcher)
    registerGameMode(dispatcher, feedback)
    registerTeleport(dispatcher, feedback)
    registerKill(dispatcher, feedback)
    registerKick(dispatcher, feedback)
    registerExperience(dispatcher, feedback)
    registerOp(dispatcher, feedback)
    registerStop(dispatcher, feedback)
}

// --- Informational ---

/** parity: `HelpCommand`. Prints brigadier's own smart usage, one line per command, slash included. */
private fun registerHelp(dispatcher: CommandDispatcher) {
    dispatcher.register("help") {
        executes { context ->
            val usable = dispatcher.usableCommands(context.source)
            for (node in usable) {
                context.source.sendSystemMessage(
                    Component.text("/" + smartUsage(node, context.source)),
                )
            }
            usable.size
        }
        argument("command", WordArgument) {
            executes { context ->
                val name = context.get<String>("command")
                val node =
                    dispatcher.usableCommands(context.source).firstOrNull { it.name == name }
                        ?: throw CommandSyntaxException(Component.translatable("commands.help.failed"))
                context.source.sendSystemMessage(Component.text("/" + smartUsage(node, context.source)))
                1
            }
        }
    }
}

/** parity: `ListPlayersCommand`. */
private fun registerList(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    dispatcher.register("list") {
        executes { context -> listPlayers(context, feedback) { Component.text(it.name) } }
        literal("uuids") {
            executes { context ->
                listPlayers(context, feedback) { player ->
                    Component.translatable(
                        "commands.list.nameAndId",
                        Component.text(player.name),
                        Component.text(player.uuid.toString()),
                    )
                }
            }
        }
    }
}

private fun listPlayers(
    context: CommandContext,
    feedback: CommandFeedback,
    format: (PlayerSession) -> Component,
): Int {
    val players = context.environment.players.snapshot()
    val names = Component.join(Component.text(", "), players.map(format))
    feedback.success(
        context.source,
        Component.translatable(
            "commands.list.players",
            Component.text(players.size.toString()),
            Component.text(context.environment.maxPlayers.toString()),
            names,
        ),
        broadcastToAdmins = false,
    )
    return players.size
}

/** parity: `SeedCommand`, which on a dedicated server needs game-master level. */
private fun registerSeed(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    dispatcher.register("seed") {
        requires(PermissionLevel.GAMEMASTERS)
        executes { context ->
            val seed = context.environment.worldSeed
            feedback.success(
                context.source,
                Component.translatable("commands.seed.success", Component.text(seed.toString())),
                broadcastToAdmins = false,
            )
            seed.toInt()
        }
    }
}

// --- Chat ---

/** parity: `SayCommand`, whose output is the `say_command` chat type (`chat.type.announcement`). */
private fun registerSay(dispatcher: CommandDispatcher) {
    dispatcher.register("say") {
        requires(PermissionLevel.GAMEMASTERS)
        argument("message", MessageArgument) {
            executes { context ->
                val message = context.get<String>("message")
                context.environment.broadcast(
                    Component.translatable(
                        "chat.type.announcement",
                        context.source.displayName,
                        Component.text(message),
                    ),
                )
                1
            }
        }
    }
}

/** parity: `EmoteCommands`, the `emote_command` chat type (`chat.type.emote`). Available to everyone. */
private fun registerMe(dispatcher: CommandDispatcher) {
    dispatcher.register("me") {
        argument("action", MessageArgument) {
            executes { context ->
                val action = context.get<String>("action")
                context.environment.broadcast(
                    Component.translatable("chat.type.emote", context.source.displayName, Component.text(action)),
                )
                1
            }
        }
    }
}

// --- Player state ---

/** parity: `GameModeCommand`. */
private fun registerGameMode(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    dispatcher.register("gamemode") {
        requires(PermissionLevel.GAMEMASTERS)
        argument("gamemode", GameModeArgument) {
            executes { context ->
                val self = context.source.player ?: throw notAPlayer()
                setGameMode(context, feedback, listOf(self), context.get("gamemode"))
            }
            argument("target", PlayersArgument(single = false)) {
                executes { context ->
                    setGameMode(context, feedback, context.get("target"), context.get("gamemode"))
                }
            }
        }
    }
}

private fun setGameMode(
    context: CommandContext,
    feedback: CommandFeedback,
    targets: List<PlayerSession>,
    mode: Gamemode,
): Int {
    var changed = 0
    val modeName = Component.translatable("gameMode.${mode.name.lowercase()}")
    for (target in targets) {
        if (target.gamemode == mode) continue
        target.connection.setGamemode(mode)
        changed++
        if (target.uuid == context.source.player?.uuid) {
            feedback.success(
                context.source,
                Component.translatable("commands.gamemode.success.self", modeName),
                broadcastToAdmins = true,
            )
        } else {
            target.sendSystemMessage(Component.translatable("gameMode.changed", modeName))
            feedback.success(
                context.source,
                Component.translatable("commands.gamemode.success.other", Component.text(target.name), modeName),
                broadcastToAdmins = true,
            )
        }
    }
    return changed
}

/** parity: `TeleportCommand`, minus its `facing`/rotation/dimension branches (documented gaps). */
private fun registerTeleport(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    val teleport =
        dispatcher.register("teleport") {
            requires(PermissionLevel.GAMEMASTERS)
            argument("location", Vec3Argument) {
                executes { context ->
                    val self = context.source.player ?: throw notAnEntity()
                    teleportToPosition(context, feedback, listOf(self), context.get("location"))
                }
            }
            argument("destination", PlayersArgument(single = true)) {
                executes { context ->
                    val self = context.source.player ?: throw notAnEntity()
                    teleportToPlayer(
                        context,
                        feedback,
                        listOf(self),
                        context.get<List<PlayerSession>>("destination").single(),
                    )
                }
            }
            argument("targets", PlayersArgument(single = false)) {
                argument("location", Vec3Argument) {
                    executes { context ->
                        teleportToPosition(context, feedback, context.get("targets"), context.get("location"))
                    }
                }
                argument("destination", PlayersArgument(single = true)) {
                    executes { context ->
                        teleportToPlayer(
                            context,
                            feedback,
                            context.get("targets"),
                            context.get<List<PlayerSession>>("destination").single(),
                        )
                    }
                }
            }
        }
    dispatcher.registerAlias("tp", teleport)
}

private fun teleportToPosition(
    context: CommandContext,
    feedback: CommandFeedback,
    targets: List<PlayerSession>,
    coordinates: Coordinates,
): Int {
    for (target in targets) {
        val (x, y, z) = coordinates.resolve(Triple(target.x, target.y, target.z))
        target.connection.teleportTo(x, y, z, target.yaw, target.pitch)
    }
    val (x, y, z) = coordinates.resolve(context.source.position)
    val message =
        if (targets.size == 1) {
            Component.translatable(
                "commands.teleport.success.location.single",
                Component.text(targets.single().name),
                Component.text(formatCoordinate(x)),
                Component.text(formatCoordinate(y)),
                Component.text(formatCoordinate(z)),
            )
        } else {
            Component.translatable(
                "commands.teleport.success.location.multiple",
                Component.text(targets.size.toString()),
                Component.text(formatCoordinate(x)),
                Component.text(formatCoordinate(y)),
                Component.text(formatCoordinate(z)),
            )
        }
    feedback.success(context.source, message, broadcastToAdmins = true)
    return targets.size
}

private fun teleportToPlayer(
    context: CommandContext,
    feedback: CommandFeedback,
    targets: List<PlayerSession>,
    destination: PlayerSession,
): Int {
    for (target in targets) {
        target.connection.teleportTo(destination.x, destination.y, destination.z, destination.yaw, destination.pitch)
    }
    val message =
        if (targets.size == 1) {
            Component.translatable(
                "commands.teleport.success.entity.single",
                Component.text(targets.single().name),
                Component.text(destination.name),
            )
        } else {
            Component.translatable(
                "commands.teleport.success.entity.multiple",
                Component.text(targets.size.toString()),
                Component.text(destination.name),
            )
        }
    feedback.success(context.source, message, broadcastToAdmins = true)
    return targets.size
}

/** parity: `KillCommand`. Vanilla's `Entity.kill` is unblockable damage, so creative mode does not save you. */
private fun registerKill(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    dispatcher.register("kill") {
        requires(PermissionLevel.GAMEMASTERS)
        executes { context ->
            val self = context.source.player ?: throw notAnEntity()
            kill(context, feedback, listOf(self))
        }
        argument("targets", PlayersArgument(single = false)) {
            executes { context -> kill(context, feedback, context.get("targets")) }
        }
    }
}

private fun kill(
    context: CommandContext,
    feedback: CommandFeedback,
    targets: List<PlayerSession>,
): Int {
    for (target in targets) target.connection.killPlayer(DEATH_GENERIC_KILL)
    val message =
        if (targets.size == 1) {
            Component.translatable("commands.kill.success.single", Component.text(targets.single().name))
        } else {
            Component.translatable("commands.kill.success.multiple", Component.text(targets.size.toString()))
        }
    feedback.success(context.source, message, broadcastToAdmins = true)
    return targets.size
}

/** parity: `KickCommand`, whose default reason is `multiplayer.disconnect.kicked`. */
private fun registerKick(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    dispatcher.register("kick") {
        requires(PermissionLevel.ADMINS)
        argument("targets", PlayersArgument(single = false)) {
            executes { context ->
                kick(context, feedback, context.get("targets"), Component.translatable("multiplayer.disconnect.kicked"))
            }
            argument("reason", MessageArgument) {
                executes { context ->
                    kick(context, feedback, context.get("targets"), Component.text(context.get<String>("reason")))
                }
            }
        }
    }
}

private fun kick(
    context: CommandContext,
    feedback: CommandFeedback,
    targets: List<PlayerSession>,
    reason: Component,
): Int {
    for (target in targets) {
        target.connection.disconnect(reason)
        feedback.success(
            context.source,
            Component.translatable("commands.kick.success", Component.text(target.name), reason),
            broadcastToAdmins = true,
        )
    }
    return targets.size
}

/** parity: `ExperienceCommand`, `/xp` alias included. */
private fun registerExperience(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    val experience =
        dispatcher.register("experience") {
            requires(PermissionLevel.GAMEMASTERS)
            literal("add") {
                argument("target", PlayersArgument(single = false)) {
                    argument("amount", IntegerArgument()) {
                        executes { context -> addExperience(context, feedback, points = true) }
                        literal("points") { executes { context -> addExperience(context, feedback, points = true) } }
                        literal("levels") { executes { context -> addExperience(context, feedback, points = false) } }
                    }
                }
            }
            literal("set") {
                argument("target", PlayersArgument(single = false)) {
                    argument("amount", IntegerArgument(min = 0)) {
                        executes { context -> setExperience(context, feedback, points = true) }
                        literal("points") { executes { context -> setExperience(context, feedback, points = true) } }
                        literal("levels") { executes { context -> setExperience(context, feedback, points = false) } }
                    }
                }
            }
            literal("query") {
                argument("target", PlayersArgument(single = true)) {
                    literal("points") { executes { context -> queryExperience(context, feedback, points = true) } }
                    literal("levels") { executes { context -> queryExperience(context, feedback, points = false) } }
                }
            }
        }
    dispatcher.registerAlias("xp", experience)
}

private fun addExperience(
    context: CommandContext,
    feedback: CommandFeedback,
    points: Boolean,
): Int {
    val targets = context.get<List<PlayerSession>>("target")
    val amount = context.get<Int>("amount")
    for (target in targets) {
        if (points) target.experience.givePoints(amount) else target.experience.giveLevels(amount)
        target.connection.syncExperience()
    }
    feedback.success(context.source, experienceMessage("add", points, amount, targets), broadcastToAdmins = true)
    return targets.size
}

private fun setExperience(
    context: CommandContext,
    feedback: CommandFeedback,
    points: Boolean,
): Int {
    val targets = context.get<List<PlayerSession>>("target")
    val amount = context.get<Int>("amount")
    var applied = 0
    for (target in targets) {
        if (points) {
            // parity: setting more points than the current level is worth is refused outright,
            // rather than silently rolling over into the next level.
            if (amount >= target.experience.xpNeededForNextLevel()) continue
            target.experience.setPoints(amount)
        } else {
            target.experience.setLevels(amount)
        }
        target.connection.syncExperience()
        applied++
    }
    if (applied == 0) throw CommandSyntaxException(Component.translatable("commands.experience.set.points.invalid"))
    feedback.success(context.source, experienceMessage("set", points, amount, targets), broadcastToAdmins = true)
    return targets.size
}

private fun experienceMessage(
    action: String,
    points: Boolean,
    amount: Int,
    targets: List<PlayerSession>,
): Component {
    val type = if (points) "points" else "levels"
    return if (targets.size == 1) {
        Component.translatable(
            "commands.experience.$action.$type.success.single",
            Component.text(amount.toString()),
            Component.text(targets.single().name),
        )
    } else {
        Component.translatable(
            "commands.experience.$action.$type.success.multiple",
            Component.text(amount.toString()),
            Component.text(targets.size.toString()),
        )
    }
}

private fun queryExperience(
    context: CommandContext,
    feedback: CommandFeedback,
    points: Boolean,
): Int {
    val target = context.get<List<PlayerSession>>("target").single()
    val result =
        if (points) {
            (target.experience.progress * target.experience.xpNeededForNextLevel()).toInt()
        } else {
            target.experience.level
        }
    feedback.success(
        context.source,
        Component.translatable(
            "commands.experience.query.${if (points) "points" else "levels"}",
            Component.text(target.name),
            Component.text(result.toString()),
        ),
        broadcastToAdmins = false,
    )
    return result
}

// --- Server ---

/**
 * parity: `OpCommand`/`DeOpCommands`, except for where the list lives: vanilla keeps operators in
 * `ops.json`, Basalt keeps them in `basalt.toml` (one configuration file, by design). The command,
 * its permission level and its messages are unchanged.
 */
private fun registerOp(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    dispatcher.register("op") {
        requires(PermissionLevel.ADMINS)
        argument("targets", GameProfileArgument) {
            executes { context ->
                val name = context.get<String>("targets")
                if (!context.environment.operators.add(name)) {
                    throw CommandSyntaxException(Component.translatable("commands.op.failed"))
                }
                context.environment.players
                    .snapshot()
                    .firstOrNull { it.name.equals(name, ignoreCase = true) }
                    ?.connection
                    ?.sendCommandTree()
                feedback.success(
                    context.source,
                    Component.translatable("commands.op.success", Component.text(name)),
                    broadcastToAdmins = true,
                )
                1
            }
        }
    }
    dispatcher.register("deop") {
        requires(PermissionLevel.ADMINS)
        argument("targets", GameProfileArgument) {
            executes { context ->
                val name = context.get<String>("targets")
                if (!context.environment.operators.remove(name)) {
                    throw CommandSyntaxException(Component.translatable("commands.deop.failed"))
                }
                context.environment.players
                    .snapshot()
                    .firstOrNull { it.name.equals(name, ignoreCase = true) }
                    ?.connection
                    ?.sendCommandTree()
                feedback.success(
                    context.source,
                    Component.translatable("commands.deop.success", Component.text(name)),
                    broadcastToAdmins = true,
                )
                1
            }
        }
    }
}

/** parity: `StopCommand`. */
private fun registerStop(
    dispatcher: CommandDispatcher,
    feedback: CommandFeedback,
) {
    dispatcher.register("stop") {
        requires(PermissionLevel.OWNERS)
        executes { context ->
            feedback.success(context.source, Component.translatable("commands.stop.stopping"), broadcastToAdmins = true)
            context.environment.stopServer()
            1
        }
    }
}

/** parity: `CommandSourceStack.getPlayerOrException`, for the forms that act on "you" as a player. */
private fun notAPlayer(): CommandSyntaxException =
    CommandSyntaxException(Component.translatable("permissions.requires.player"))

/** parity: `CommandSourceStack.getEntityOrException`, for the forms that act on "you" as an entity. */
private fun notAnEntity(): CommandSyntaxException =
    CommandSyntaxException(Component.translatable("permissions.requires.entity"))

/** parity: `TeleportCommand.formatDouble`, which is `String.format("%f")`: always six decimals. */
private fun formatCoordinate(value: Double): String {
    val scaled = (abs(value) * 1_000_000.0).roundToLong()
    val whole = scaled / 1_000_000
    val fraction = scaled % 1_000_000
    val sign = if (value < 0) "-" else ""
    return "$sign$whole." + fraction.toString().padStart(6, '0')
}

/** parity: `DamageTypes.GENERIC_KILL`'s message id, which the client renders as "<name> died". */
private const val DEATH_GENERIC_KILL = "genericKill"
