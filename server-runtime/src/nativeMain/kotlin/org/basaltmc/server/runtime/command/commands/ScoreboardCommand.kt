package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.ComponentArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.ObjectiveArgument
import org.basaltmc.server.runtime.command.ObjectiveCriteriaArgument
import org.basaltmc.server.runtime.command.OperationArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.ScoreHolderArgument
import org.basaltmc.server.runtime.command.ScoreboardSlotArgument
import org.basaltmc.server.runtime.command.StyleArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.scoreboard.Objective
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.wire.protocol.play.DisplaySlot
import org.basaltmc.wire.protocol.play.NumberFormat
import org.basaltmc.wire.protocol.play.ObjectiveRenderType
import org.basaltmc.wire.protocol.play.ResetScorePacket
import org.basaltmc.wire.protocol.play.SetDisplayObjectivePacket
import org.basaltmc.wire.protocol.play.SetObjectivePacket
import org.basaltmc.wire.protocol.play.SetScorePacket
import org.basaltmc.wire.text.Component

/**
 * parity: `ScoreboardCommand`, both halves: `objectives` and `players`.
 *
 * Objectives are created with a criterion but nothing increments them yet (see [Objective]), so in
 * practice every objective behaves as `dummy` and `/trigger` is the one that moves on its own.
 */
public class ScoreboardCommand(
    private val scoreboard: Scoreboard,
) : Command(name = "scoreboard", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("objectives") {
            literal("list") { executes { context -> listObjectives(context) } }
            literal("add") {
                argument("objective", WordArgument) {
                    argument("criteria", ObjectiveCriteriaArgument) {
                        executes { context -> addObjective(context, null) }
                        argument("displayName", ComponentArgument) {
                            executes { context -> addObjective(context, context.get("displayName")) }
                        }
                    }
                }
            }
            literal("remove") {
                objective { executes { context -> removeObjective(context) } }
            }
            literal("setdisplay") {
                argument("slot", ScoreboardSlotArgument) {
                    executes { context -> setDisplay(context, context.get("slot"), null) }
                    objective {
                        executes { context -> setDisplay(context, context.get("slot"), context.get("objective")) }
                    }
                }
            }
            literal("modify") {
                objective {
                    literal("displayname") {
                        argument("displayName", ComponentArgument) {
                            executes { context -> modifyDisplayName(context) }
                        }
                    }
                    literal("rendertype") {
                        for (type in ObjectiveRenderType.entries) {
                            literal(type.name.lowercase()) { executes { context -> modifyRenderType(context, type) } }
                        }
                    }
                    literal("displayautoupdate") {
                        argument("value", BoolArgument) { executes { context -> modifyAutoUpdate(context) } }
                    }
                    literal("numberformat") {
                        executes { context -> modifyNumberFormat(context, null) }
                        numberFormats { context, format -> modifyNumberFormat(context, format) }
                    }
                }
            }
        }
        root.literal("players") {
            literal("list") {
                executes { context -> listHolders(context) }
                argument("target", ScoreHolderArgument) { executes { context -> listScoresOf(context) } }
            }
            literal("get") {
                argument("target", ScoreHolderArgument) {
                    objective { executes { context -> getScore(context) } }
                }
            }
            literal("display") {
                literal("name") {
                    argument("targets", ScoreHolderArgument) {
                        objective {
                            executes { context -> displayName(context, null) }
                            argument("name", ComponentArgument) {
                                executes { context -> displayName(context, context.get("name")) }
                            }
                        }
                    }
                }
                literal("numberformat") {
                    argument("targets", ScoreHolderArgument) {
                        objective {
                            executes { context -> displayFormat(context, null) }
                            numberFormats { context, format -> displayFormat(context, format) }
                        }
                    }
                }
            }
            literal("set") {
                argument("targets", ScoreHolderArgument) {
                    objective {
                        argument("score", IntegerArgument()) { executes { context -> setScore(context) } }
                    }
                }
            }
            literal("add") {
                argument("targets", ScoreHolderArgument) {
                    objective {
                        argument("score", IntegerArgument(min = 0)) { executes { context -> changeScore(context, 1) } }
                    }
                }
            }
            literal("remove") {
                argument("targets", ScoreHolderArgument) {
                    objective {
                        argument("score", IntegerArgument(min = 0)) { executes { context -> changeScore(context, -1) } }
                    }
                }
            }
            literal("reset") {
                argument("targets", ScoreHolderArgument) {
                    executes { context -> reset(context, null) }
                    objective { executes { context -> reset(context, context.get("objective")) } }
                }
            }
            literal("enable") {
                argument("targets", ScoreHolderArgument) {
                    objective { executes { context -> enable(context) } }
                }
            }
            literal("operation") {
                argument("targets", ScoreHolderArgument) {
                    objective("targetObjective") {
                        argument("operation", OperationArgument) {
                            argument("source", ScoreHolderArgument) {
                                objective("sourceObjective") {
                                    executes { context -> operation(context) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /** An argument naming an objective that already exists, so it completes with the ones that do. */
    private fun CommandNodeBuilder.objective(
        name: String = "objective",
        build: CommandNodeBuilder.() -> Unit,
    ) {
        argument(name, ObjectiveArgument) { build() }
    }

    private fun objectiveOf(
        context: CommandContext,
        name: String = "objective",
    ): Objective {
        val id = context.get<String>(name)
        return scoreboard[id]
            ?: throw CommandSyntaxException(
                Component.translatable("arguments.objective.notFound", Component.text(id)),
            )
    }

    private fun listObjectives(context: CommandContext): Int {
        val objectives = scoreboard.allObjectives
        context.success(
            if (objectives.isEmpty()) {
                Component.translatable("commands.scoreboard.objectives.list.empty")
            } else {
                Component.translatable(
                    "commands.scoreboard.objectives.list.success",
                    Component.text(objectives.size.toString()),
                    Component.text(objectives.joinToString(", ") { it.name }),
                )
            },
        )
        return objectives.size
    }

    private fun addObjective(
        context: CommandContext,
        displayName: Component?,
    ): Int {
        val name = context.get<String>("objective")
        if (name.length > MAX_NAME_LENGTH) {
            throw CommandSyntaxException(
                Component.text("An objective name may be at most $MAX_NAME_LENGTH characters."),
            )
        }
        val objective =
            Objective(name, context.get("criteria"), displayName ?: Component.text(name))
        if (!scoreboard.add(objective)) {
            throw CommandSyntaxException(Component.translatable("commands.scoreboard.objectives.add.duplicate"))
        }
        broadcast(context) {
            SetObjectivePacket(
                objective.name,
                SetObjectivePacket.METHOD_ADD,
                objective.displayName,
                objective.renderType,
            )
        }
        context.success(
            Component.translatable("commands.scoreboard.objectives.add.success", objective.displayName),
            broadcastToAdmins = true,
        )
        return scoreboard.allObjectives.size
    }

    private fun removeObjective(context: CommandContext): Int {
        val objective = objectiveOf(context)
        scoreboard.remove(objective.name)
        broadcast(context) { SetObjectivePacket(objective.name, SetObjectivePacket.METHOD_REMOVE) }
        context.success(
            Component.translatable("commands.scoreboard.objectives.remove.success", objective.displayName),
            broadcastToAdmins = true,
        )
        return scoreboard.allObjectives.size
    }

    private fun setDisplay(
        context: CommandContext,
        slot: DisplaySlot,
        objectiveName: String?,
    ): Int {
        val current = scoreboard.displayed(slot)
        if (objectiveName == null) {
            if (current == null) {
                throw CommandSyntaxException(
                    Component.translatable("commands.scoreboard.objectives.display.alreadyEmpty"),
                )
            }
            scoreboard.display(slot, null)
            broadcast(context) { SetDisplayObjectivePacket(slot, "") }
            context.success(
                Component.translatable(
                    "commands.scoreboard.objectives.display.cleared",
                    Component.text(slot.name.lowercase()),
                ),
                broadcastToAdmins = true,
            )
            return 0
        }
        val objective = objectiveOf(context)
        if (current == objective.name) {
            throw CommandSyntaxException(Component.translatable("commands.scoreboard.objectives.display.alreadySet"))
        }
        scoreboard.display(slot, objective.name)
        broadcast(context) { SetDisplayObjectivePacket(slot, objective.name) }
        // Whatever was already scored has to be sent again: the client only keeps lines for an
        // objective it is currently showing.
        for (holder in scoreboard.holdersIn(objective.name)) {
            val score = scoreboard.score(holder, objective.name) ?: continue
            broadcast(context) { SetScorePacket(holder, objective.name, score) }
        }
        context.success(
            Component.translatable(
                "commands.scoreboard.objectives.display.set",
                Component.text(slot.name.lowercase()),
                objective.displayName,
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    /** The three shapes a number format takes, declared once for both places that accept one. */
    private fun CommandNodeBuilder.numberFormats(run: (CommandContext, NumberFormat) -> Int) {
        literal("blank") { executes { context -> run(context, NumberFormat.Blank) } }
        literal("fixed") {
            argument("contents", ComponentArgument) {
                executes { context -> run(context, NumberFormat.Fixed(context.get("contents"))) }
            }
        }
        literal("styled") {
            argument("style", StyleArgument) {
                executes { context -> run(context, NumberFormat.Styled(context.get("style"))) }
            }
        }
    }

    private fun modifyAutoUpdate(context: CommandContext): Int {
        val objective = objectiveOf(context)
        objective.displayAutoUpdate = context.get("value")
        context.success(
            Component.translatable("commands.scoreboard.objectives.modify.displayautoupdate", objective.displayName),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun modifyNumberFormat(
        context: CommandContext,
        format: NumberFormat?,
    ): Int {
        val objective = objectiveOf(context)
        objective.numberFormat = format
        resend(context, objective)
        context.success(
            Component.translatable(
                if (format == null) {
                    "commands.scoreboard.objectives.modify.reset.numberFormat"
                } else {
                    "commands.scoreboard.objectives.modify.numberFormat"
                },
                objective.displayName,
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun displayName(
        context: CommandContext,
        name: Component?,
    ): Int {
        val objective = objectiveOf(context)
        val holders = context.get<List<String>>("targets")
        for (holder in holders) scoreboard.setDisplayName(holder, objective.name, name)
        for (holder in holders) resendScore(context, holder, objective)
        context.success(
            Component.translatable(
                if (name ==
                    null
                ) {
                    "commands.scoreboard.players.display.name.clear.success.single"
                } else {
                    "commands.scoreboard.players.display.name.success.single"
                },
                Component.text(holders.firstOrNull().orEmpty()),
                objective.displayName,
            ),
            broadcastToAdmins = true,
        )
        return holders.size
    }

    private fun displayFormat(
        context: CommandContext,
        format: NumberFormat?,
    ): Int {
        val objective = objectiveOf(context)
        val holders = context.get<List<String>>("targets")
        for (holder in holders) scoreboard.setNumberFormat(holder, objective.name, format)
        for (holder in holders) resendScore(context, holder, objective)
        context.success(
            Component.translatable(
                if (format ==
                    null
                ) {
                    "commands.scoreboard.players.display.numberFormat.clear.success.single"
                } else {
                    "commands.scoreboard.players.display.numberFormat.success.single"
                },
                Component.text(holders.firstOrNull().orEmpty()),
                objective.displayName,
            ),
            broadcastToAdmins = true,
        )
        return holders.size
    }

    private fun modifyDisplayName(context: CommandContext): Int {
        val objective = objectiveOf(context)
        objective.displayName = context.get("displayName")
        broadcast(context) {
            SetObjectivePacket(
                objective.name,
                SetObjectivePacket.METHOD_CHANGE,
                objective.displayName,
                objective.renderType,
            )
        }
        context.success(
            Component.translatable(
                "commands.scoreboard.objectives.modify.displayname",
                Component.text(objective.name),
                objective.displayName,
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    /** Re-sends the objective so a client redraws it with whatever changed. */
    private fun resend(
        context: CommandContext,
        objective: Objective,
    ) {
        broadcast(context) {
            SetObjectivePacket(
                objective.name,
                SetObjectivePacket.METHOD_CHANGE,
                objective.displayName,
                objective.renderType,
                objective.numberFormat,
            )
        }
    }

    private fun resendScore(
        context: CommandContext,
        holder: String,
        objective: Objective,
    ) {
        val value = scoreboard.score(holder, objective.name) ?: return
        broadcast(context) {
            SetScorePacket(
                holder,
                objective.name,
                value,
                scoreboard.displayNameOf(holder, objective.name),
                scoreboard.numberFormatOf(holder, objective.name),
            )
        }
    }

    private fun modifyRenderType(
        context: CommandContext,
        type: ObjectiveRenderType,
    ): Int {
        val objective = objectiveOf(context)
        objective.renderType = type
        broadcast(context) {
            SetObjectivePacket(
                objective.name,
                SetObjectivePacket.METHOD_CHANGE,
                objective.displayName,
                objective.renderType,
            )
        }
        context.success(
            Component.translatable("commands.scoreboard.objectives.modify.rendertype", objective.displayName),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun listHolders(context: CommandContext): Int {
        val holders = scoreboard.holders()
        context.success(
            if (holders.isEmpty()) {
                Component.translatable("commands.scoreboard.players.list.empty")
            } else {
                Component.translatable(
                    "commands.scoreboard.players.list.success",
                    Component.text(holders.size.toString()),
                    Component.text(holders.joinToString(", ")),
                )
            },
        )
        return holders.size
    }

    private fun listScoresOf(context: CommandContext): Int {
        val holder = context.get<List<String>>("target").first()
        val scores = scoreboard.scoresOf(holder)
        context.success(
            if (scores.isEmpty()) {
                Component.translatable("commands.scoreboard.players.list.entity.empty", Component.text(holder))
            } else {
                Component.translatable(
                    "commands.scoreboard.players.list.entity.success",
                    Component.text(holder),
                    Component.text(scores.size.toString()),
                )
            },
        )
        return scores.size
    }

    private fun getScore(context: CommandContext): Int {
        val holder = context.get<List<String>>("target").first()
        val objective = objectiveOf(context)
        val score =
            scoreboard.score(holder, objective.name)
                ?: throw CommandSyntaxException(
                    Component.translatable(
                        "commands.scoreboard.players.get.null",
                        objective.displayName,
                        Component.text(holder),
                    ),
                )
        context.success(
            Component.translatable(
                "commands.scoreboard.players.get.success",
                Component.text(holder),
                Component.text(score.toString()),
                objective.displayName,
            ),
        )
        return score
    }

    private fun setScore(context: CommandContext): Int {
        val holders = context.get<List<String>>("targets")
        val objective = objectiveOf(context)
        val value = context.get<Int>("score")
        for (holder in holders) {
            scoreboard.setScore(holder, objective.name, value)
            sendScore(context, holder, objective.name, value)
        }
        report(context, holders, objective, "set", value)
        return value * holders.size
    }

    private fun changeScore(
        context: CommandContext,
        sign: Int,
    ): Int {
        val holders = context.get<List<String>>("targets")
        val objective = objectiveOf(context)
        val delta = context.get<Int>("score") * sign
        var total = 0
        for (holder in holders) {
            val value = scoreboard.addScore(holder, objective.name, delta)
            sendScore(context, holder, objective.name, value)
            total += value
        }
        report(context, holders, objective, if (sign > 0) "add" else "remove", delta * sign)
        return total
    }

    private fun reset(
        context: CommandContext,
        objectiveName: String?,
    ): Int {
        val holders = context.get<List<String>>("targets")
        for (holder in holders) {
            scoreboard.resetScore(holder, objectiveName)
            broadcast(context) { ResetScorePacket(holder, objectiveName) }
        }
        val key = if (objectiveName == null) "all" else "specific"
        context.success(
            if (holders.size == 1) {
                Component.translatable(
                    "commands.scoreboard.players.reset.$key.single",
                    Component.text(holders.single()),
                )
            } else {
                Component.translatable(
                    "commands.scoreboard.players.reset.$key.multiple",
                    Component.text(holders.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return holders.size
    }

    private fun enable(context: CommandContext): Int {
        val holders = context.get<List<String>>("targets")
        val objective = objectiveOf(context)
        if (!objective.isTrigger) {
            throw CommandSyntaxException(Component.translatable("commands.scoreboard.players.enable.invalid"))
        }
        for (holder in holders) {
            scoreboard.enableTrigger(holder, objective.name)
            if (scoreboard.score(holder, objective.name) == null) {
                scoreboard.setScore(holder, objective.name, 0)
                sendScore(context, holder, objective.name, 0)
            }
        }
        context.success(
            if (holders.size == 1) {
                Component.translatable(
                    "commands.scoreboard.players.enable.success.single",
                    objective.displayName,
                    Component.text(holders.single()),
                )
            } else {
                Component.translatable(
                    "commands.scoreboard.players.enable.success.multiple",
                    objective.displayName,
                    Component.text(holders.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return holders.size
    }

    private fun operation(context: CommandContext): Int {
        val holders = context.get<List<String>>("targets")
        val target = objectiveOf(context, "targetObjective")
        val sourceHolder = context.get<List<String>>("source").first()
        val sourceObjective = objectiveOf(context, "sourceObjective")
        val operation = context.get<String>("operation")
        val sourceValue = scoreboard.score(sourceHolder, sourceObjective.name) ?: 0

        var total = 0
        for (holder in holders) {
            val current = scoreboard.score(holder, target.name) ?: 0
            val result =
                when (operation) {
                    "=" -> sourceValue
                    "+=" -> current + sourceValue
                    "-=" -> current - sourceValue
                    "*=" -> current * sourceValue
                    "/=" ->
                        if (sourceValue == 0) {
                            throw CommandSyntaxException(
                                Component.translatable("arguments.operation.div0"),
                            )
                        } else {
                            // parity: vanilla floors, so -7 / 2 is -4, not -3.
                            kotlin.math.floor(current.toDouble() / sourceValue).toInt()
                        }
                    "%=" ->
                        if (sourceValue == 0) {
                            throw CommandSyntaxException(
                                Component.translatable("arguments.operation.div0"),
                            )
                        } else {
                            current.mod(sourceValue)
                        }
                    "<" -> minOf(current, sourceValue)
                    ">" -> maxOf(current, sourceValue)
                    "><" -> {
                        scoreboard.setScore(sourceHolder, sourceObjective.name, current)
                        sendScore(context, sourceHolder, sourceObjective.name, current)
                        sourceValue
                    }
                    else -> throw CommandSyntaxException(Component.translatable("arguments.operation.invalid"))
                }
            scoreboard.setScore(holder, target.name, result)
            sendScore(context, holder, target.name, result)
            total += result
        }
        context.success(
            if (holders.size == 1) {
                Component.translatable(
                    "commands.scoreboard.players.operation.success.single",
                    target.displayName,
                    Component.text(holders.single()),
                    Component.text(scoreboard.score(holders.single(), target.name).toString()),
                )
            } else {
                Component.translatable(
                    "commands.scoreboard.players.operation.success.multiple",
                    target.displayName,
                    Component.text(holders.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return total
    }

    private fun report(
        context: CommandContext,
        holders: List<String>,
        objective: Objective,
        action: String,
        value: Int,
    ) {
        context.success(
            if (holders.size == 1) {
                Component.translatable(
                    "commands.scoreboard.players.$action.success.single",
                    Component.text(value.toString()),
                    objective.displayName,
                    Component.text(holders.single()),
                )
            } else {
                Component.translatable(
                    "commands.scoreboard.players.$action.success.multiple",
                    Component.text(value.toString()),
                    objective.displayName,
                    Component.text(holders.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
    }

    /** A score only reaches clients while its objective is on screen somewhere. */
    private fun sendScore(
        context: CommandContext,
        holder: String,
        objectiveName: String,
        value: Int,
    ) {
        if (DisplaySlot.entries.none { scoreboard.displayed(it) == objectiveName }) return
        broadcast(context) { SetScorePacket(holder, objectiveName, value) }
    }

    private inline fun <reified T> broadcast(
        context: CommandContext,
        packet: () -> T,
    ) {
        val players = context.environment.players.snapshot()
        if (players.isEmpty()) return
        when (val built = packet()) {
            is SetObjectivePacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetObjectivePacket.ID) { built.write(it) }
                }
            is SetDisplayObjectivePacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetDisplayObjectivePacket.ID) { built.write(it) }
                }
            is SetScorePacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetScorePacket.ID) { built.write(it) }
                }
            is ResetScorePacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(ResetScorePacket.ID) { built.write(it) }
                }
            else -> error("unsupported scoreboard packet $built")
        }
    }

    private companion object {
        /** parity: vanilla's own cap on an objective name. */
        const val MAX_NAME_LENGTH = 32768
    }
}
