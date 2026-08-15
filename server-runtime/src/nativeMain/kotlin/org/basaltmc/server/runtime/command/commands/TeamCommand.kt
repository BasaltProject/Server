package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.ComponentArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.ScoreHolderArgument
import org.basaltmc.server.runtime.command.TeamArgument
import org.basaltmc.server.runtime.command.TeamColorArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.scoreboard.Team
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.wire.protocol.play.CollisionRule
import org.basaltmc.wire.protocol.play.NameTagVisibility
import org.basaltmc.wire.protocol.play.SetPlayerTeamPacket
import org.basaltmc.wire.protocol.play.TeamColor
import org.basaltmc.wire.text.Component

/**
 * parity: `TeamCommand`, minus `deathMessageVisibility`, which needs death messages to be filtered
 * per team and so belongs with the death message work rather than here.
 */
public class TeamCommand(
    private val teams: Teams,
) : Command(name = "team", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("list") {
            executes { context -> listTeams(context) }
            team { executes { context -> listMembers(context) } }
        }
        root.literal("add") {
            argument("team", WordArgument) {
                executes { context -> add(context, null) }
                argument("displayName", ComponentArgument) {
                    executes { context -> add(context, context.get("displayName")) }
                }
            }
        }
        root.literal("remove") {
            team { executes { context -> remove(context) } }
        }
        root.literal("empty") {
            team { executes { context -> empty(context) } }
        }
        root.literal("join") {
            team {
                executes { context -> join(context, listOf(context.requirePlayer().name)) }
                argument("members", ScoreHolderArgument) {
                    executes { context -> join(context, context.get("members")) }
                }
            }
        }
        root.literal("leave") {
            argument("members", ScoreHolderArgument) { executes { context -> leave(context) } }
        }
        root.literal("modify") {
            team {
                literal("displayName") {
                    argument("displayName", ComponentArgument) {
                        executes { context -> modifyName(context) }
                    }
                }
                literal("prefix") {
                    argument("prefix", ComponentArgument) { executes { context -> modifyPrefix(context) } }
                }
                literal("suffix") {
                    argument("suffix", ComponentArgument) { executes { context -> modifySuffix(context) } }
                }
                literal("color") {
                    literal("reset") { executes { context -> modifyColor(context, null) } }
                    argument("value", TeamColorArgument) {
                        executes { context -> modifyColor(context, context.get("value")) }
                    }
                }
                literal("friendlyFire") {
                    argument("allowed", BoolArgument) { executes { context -> modifyFriendlyFire(context) } }
                }
                literal("seeFriendlyInvisibles") {
                    argument("allowed", BoolArgument) { executes { context -> modifySeeInvisibles(context) } }
                }
                literal("nametagVisibility") {
                    for (visibility in NameTagVisibility.entries) {
                        literal(visibility.serializedName) {
                            executes { context -> modifyNameTagVisibility(context, visibility) }
                        }
                    }
                }
                literal("collisionRule") {
                    for (rule in CollisionRule.entries) {
                        literal(rule.serializedName) { executes { context -> modifyCollision(context, rule) } }
                    }
                }
            }
        }
    }

    /** An argument naming a team that already exists, so it completes with the ones that do. */
    private fun CommandNodeBuilder.team(build: CommandNodeBuilder.() -> Unit) {
        argument("team", TeamArgument) { build() }
    }

    private fun teamOf(context: CommandContext): Team {
        val name = context.get<String>("team")
        return teams[name]
            ?: throw CommandSyntaxException(
                Component.translatable("team.notFound", Component.text(name)),
            )
    }

    private fun listTeams(context: CommandContext): Int {
        val all = teams.all
        context.success(
            if (all.isEmpty()) {
                Component.translatable("commands.team.list.teams.empty")
            } else {
                Component.translatable(
                    "commands.team.list.teams.success",
                    Component.text(all.size.toString()),
                    Component.text(all.joinToString(", ") { it.name }),
                )
            },
        )
        return all.size
    }

    private fun listMembers(context: CommandContext): Int {
        val team = teamOf(context)
        context.success(
            if (team.members.isEmpty()) {
                Component.translatable("commands.team.list.members.empty", team.displayName)
            } else {
                Component.translatable(
                    "commands.team.list.members.success",
                    team.displayName,
                    Component.text(team.members.size.toString()),
                    Component.text(team.members.joinToString(", ")),
                )
            },
        )
        return team.members.size
    }

    private fun add(
        context: CommandContext,
        displayName: Component?,
    ): Int {
        val name = context.get<String>("team")
        val team =
            teams.add(name)
                ?: throw CommandSyntaxException(Component.translatable("commands.team.add.duplicate"))
        displayName?.let { team.displayName = it }
        broadcast(context, SetPlayerTeamPacket(team.name, SetPlayerTeamPacket.METHOD_ADD, team.parameters()))
        context.success(
            Component.translatable("commands.team.add.success", team.displayName),
            broadcastToAdmins = true,
        )
        return teams.all.size
    }

    private fun remove(context: CommandContext): Int {
        val team = teamOf(context)
        teams.remove(team.name)
        broadcast(context, SetPlayerTeamPacket(team.name, SetPlayerTeamPacket.METHOD_REMOVE))
        context.success(
            Component.translatable("commands.team.remove.success", team.displayName),
            broadcastToAdmins = true,
        )
        return teams.all.size
    }

    private fun empty(context: CommandContext): Int {
        val team = teamOf(context)
        if (team.members.isEmpty()) {
            throw CommandSyntaxException(Component.translatable("commands.team.empty.unchanged"))
        }
        val removed = team.members.toList()
        team.members.clear()
        broadcast(context, SetPlayerTeamPacket(team.name, SetPlayerTeamPacket.METHOD_LEAVE, members = removed))
        context.success(
            Component.translatable(
                "commands.team.empty.success",
                Component.text(removed.size.toString()),
                team.displayName,
            ),
            broadcastToAdmins = true,
        )
        return removed.size
    }

    private fun join(
        context: CommandContext,
        members: List<String>,
    ): Int {
        val team = teamOf(context)
        for (member in members) {
            val previous = teams.join(team, member)
            if (previous != null && previous.name != team.name) {
                broadcast(
                    context,
                    SetPlayerTeamPacket(previous.name, SetPlayerTeamPacket.METHOD_LEAVE, members = listOf(member)),
                )
            }
        }
        broadcast(context, SetPlayerTeamPacket(team.name, SetPlayerTeamPacket.METHOD_JOIN, members = members))
        context.success(
            if (members.size == 1) {
                Component.translatable(
                    "commands.team.join.success.single",
                    Component.text(members.single()),
                    team.displayName,
                )
            } else {
                Component.translatable(
                    "commands.team.join.success.multiple",
                    Component.text(members.size.toString()),
                    team.displayName,
                )
            },
            broadcastToAdmins = true,
        )
        return members.size
    }

    private fun leave(context: CommandContext): Int {
        val members = context.get<List<String>>("members")
        var left = 0
        for (member in members) {
            val previous = teams.leave(member) ?: continue
            left++
            broadcast(
                context,
                SetPlayerTeamPacket(previous.name, SetPlayerTeamPacket.METHOD_LEAVE, members = listOf(member)),
            )
        }
        context.success(
            if (members.size == 1) {
                Component.translatable("commands.team.leave.success.single", Component.text(members.single()))
            } else {
                Component.translatable("commands.team.leave.success.multiple", Component.text(members.size.toString()))
            },
            broadcastToAdmins = true,
        )
        return left
    }

    private fun modifyName(context: CommandContext): Int {
        val team = teamOf(context)
        val name = context.get<Component>("displayName")
        if (team.displayName == name) throw unchanged("name")
        team.displayName = name
        changed(context, team)
        context.success(
            Component.translatable("commands.team.option.name.success", team.displayName),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun modifyPrefix(context: CommandContext): Int {
        val team = teamOf(context)
        team.prefix = context.get("prefix")
        changed(context, team)
        context.success(Component.translatable("commands.team.option.prefix.success", team.prefix))
        return 0
    }

    private fun modifySuffix(context: CommandContext): Int {
        val team = teamOf(context)
        team.suffix = context.get("suffix")
        changed(context, team)
        context.success(Component.translatable("commands.team.option.suffix.success", team.suffix))
        return 0
    }

    private fun modifyColor(
        context: CommandContext,
        color: TeamColor?,
    ): Int {
        val team = teamOf(context)
        if (team.color == color) throw unchanged("color")
        team.color = color
        changed(context, team)
        context.success(
            if (color == null) {
                Component.translatable("commands.team.option.color.clear.success", team.displayName)
            } else {
                Component.translatable(
                    "commands.team.option.color.success",
                    team.displayName,
                    Component.text(color.name.lowercase()),
                )
            },
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun modifyFriendlyFire(context: CommandContext): Int {
        val team = teamOf(context)
        val allowed = context.get<Boolean>("allowed")
        if (team.friendlyFire == allowed) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.team.option.friendlyfire.already" + if (allowed) "Enabled" else "Disabled",
                ),
            )
        }
        team.friendlyFire = allowed
        changed(context, team)
        context.success(
            Component.translatable(
                "commands.team.option.friendlyfire." + if (allowed) "enabled" else "disabled",
                team.displayName,
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun modifySeeInvisibles(context: CommandContext): Int {
        val team = teamOf(context)
        val allowed = context.get<Boolean>("allowed")
        if (team.seeFriendlyInvisibles == allowed) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.team.option.seeFriendlyInvisibles.already" + if (allowed) "Enabled" else "Disabled",
                ),
            )
        }
        team.seeFriendlyInvisibles = allowed
        changed(context, team)
        context.success(
            Component.translatable(
                "commands.team.option.seeFriendlyInvisibles." + if (allowed) "enabled" else "disabled",
                team.displayName,
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun modifyNameTagVisibility(
        context: CommandContext,
        visibility: NameTagVisibility,
    ): Int {
        val team = teamOf(context)
        if (team.nameTagVisibility == visibility) throw unchanged("nametagVisibility")
        team.nameTagVisibility = visibility
        changed(context, team)
        context.success(
            Component.translatable(
                "commands.team.option.nametagVisibility.success",
                team.displayName,
                Component.text(visibility.serializedName),
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun modifyCollision(
        context: CommandContext,
        rule: CollisionRule,
    ): Int {
        val team = teamOf(context)
        if (team.collisionRule == rule) throw unchanged("collisionRule")
        team.collisionRule = rule
        changed(context, team)
        context.success(
            Component.translatable(
                "commands.team.option.collisionRule.success",
                team.displayName,
                Component.text(rule.serializedName),
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun changed(
        context: CommandContext,
        team: Team,
    ) {
        broadcast(context, SetPlayerTeamPacket(team.name, SetPlayerTeamPacket.METHOD_CHANGE, team.parameters()))
    }

    private fun broadcast(
        context: CommandContext,
        packet: SetPlayerTeamPacket,
    ) {
        for (player in context.environment.players.snapshot()) {
            player.connection.sendPlayPacket(SetPlayerTeamPacket.ID) { packet.write(it) }
        }
    }

    private fun unchanged(what: String) =
        CommandSyntaxException(Component.translatable("commands.team.option.$what.unchanged"))
}
