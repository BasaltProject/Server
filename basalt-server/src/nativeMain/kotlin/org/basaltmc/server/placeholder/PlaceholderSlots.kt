package org.basaltmc.server.placeholder

import org.basaltmc.common.placeholder.PlaceholderSlot
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.server.config.PingRequest
import org.basaltmc.server.player.PlayerSession

/**
 * What a placeholder can be resolved against here.
 *
 * Two situations, and they are not the same one with pieces missing. [PLAYER] is somebody logged in,
 * with a position and a game mode. [PING] is a connection that has sent a handshake and nothing
 * else, where there is no player at all and never will be unless they go on to log in. A placeholder
 * about a player's coordinates is answerable in the first and not in the second, and saying so is
 * the whole reason contexts are typed rather than one bag of nullable fields.
 */
public object PlaceholderSlots {
    public val PLAYER: PlaceholderSlot<PlayerSession> = PlaceholderSlot("player")
    public val PING: PlaceholderSlot<PingRequest> = PlaceholderSlot("ping")

    /**
     * The second player of a relational placeholder: "what does A see about B".
     *
     * Nothing Basalt registers reads it yet, and it exists anyway, because a relational placeholder
     * has nowhere to put the other side without it. A plugin comparing two players (their team, who
     * can see whom) needs exactly this slot and cannot add one from outside.
     */
    public val OTHER_PLAYER: PlaceholderSlot<PlayerSession> = PlaceholderSlot("other_player")
}

/**
 * A placeholder that exists, is spelled correctly, and cannot be answered yet because the system
 * behind it is not written.
 *
 * Registered rather than left out on purpose. Leaving it out reports "no such placeholder", which
 * reads as a typo and sends the operator looking for one; this says what is actually missing. It is
 * the same rule the rest of the project follows for parity gaps: declare the gap, never approximate
 * around it.
 */
internal fun notImplemented(reason: String): PlaceholderValue = PlaceholderValue.Unavailable(reason)
