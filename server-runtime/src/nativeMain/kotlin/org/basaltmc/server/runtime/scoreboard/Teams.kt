package org.basaltmc.server.runtime.scoreboard

import org.basaltmc.wire.protocol.play.CollisionRule
import org.basaltmc.wire.protocol.play.NameTagVisibility
import org.basaltmc.wire.protocol.play.SetPlayerTeamPacket
import org.basaltmc.wire.protocol.play.TeamColor
import org.basaltmc.wire.text.Component

/** One team, with everything `/team modify` can change about it. parity: `PlayerTeam`. */
public class Team(
    public val name: String,
) {
    public var displayName: Component = Component.text(name)
    public var prefix: Component = Component.empty()
    public var suffix: Component = Component.empty()
    public var color: TeamColor? = null
    public var friendlyFire: Boolean = true
    public var seeFriendlyInvisibles: Boolean = true
    public var nameTagVisibility: NameTagVisibility = NameTagVisibility.ALWAYS

    /** parity: `deathMessageVisibility`, which decides who reads this team's death messages. */
    public var deathMessageVisibility: NameTagVisibility = NameTagVisibility.ALWAYS
    public var collisionRule: CollisionRule = CollisionRule.ALWAYS

    /** Member names, which for a player is their name and for anything else its UUID, as vanilla does. */
    public val members: MutableSet<String> = linkedSetOf()

    public fun parameters(): SetPlayerTeamPacket.Parameters =
        SetPlayerTeamPacket.Parameters(
            displayName = displayName,
            prefix = prefix,
            suffix = suffix,
            nameTagVisibility = nameTagVisibility,
            collisionRule = collisionRule,
            color = color,
            friendlyFire = friendlyFire,
            seeFriendlyInvisibles = seeFriendlyInvisibles,
        )
}

/** Every team on the server, and who is in them. A member belongs to at most one, as in vanilla. */
public class Teams {
    private val teams = linkedMapOf<String, Team>()

    public val all: List<Team> get() = teams.values.toList()

    public operator fun get(name: String): Team? = teams[name]

    public fun add(name: String): Team? {
        if (name in teams) return null
        return Team(name).also { teams[name] = it }
    }

    public fun remove(name: String): Team? = teams.remove(name)

    public fun teamOf(member: String): Team? = teams.values.firstOrNull { member in it.members }

    /** Puts [member] in [team], taking them out of whatever team they were in. Returns the old one. */
    public fun join(
        team: Team,
        member: String,
    ): Team? {
        val previous = teamOf(member)
        previous?.members?.remove(member)
        team.members += member
        return previous
    }

    public fun leave(member: String): Team? {
        val previous = teamOf(member) ?: return null
        previous.members.remove(member)
        return previous
    }
}
