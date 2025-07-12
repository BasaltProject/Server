package org.basaltmc.server.plugins

/**
 * One packet on its way past a plugin.
 *
 * Reused between deliveries the way [PluginEvent] is, for the same reason: a packet listener sits on
 * the busiest path in the server, and a table per packet per player is not a cost anybody would
 * accept for the ability to watch one.
 */
public class PluginPacket {
    public var name: String = ""

    /** True on the way in, false on the way out. */
    public var serverbound: Boolean = false

    public var playerName: String = ""

    public var playerUuid: String = ""

    /**
     * The packet's own bytes, without the id in front.
     *
     * The escape hatch, not the road: a plugin that only wants to see one packet go by should not
     * have to decode it, and one that wants a field this build does not name yet can still have it.
     */
    public var bytes: ByteArray = EMPTY

    public var cancelled: Boolean = false

    /** Set when a listener replaced the body. Null means it goes on unchanged. */
    public var replacement: ByteArray? = null

    public fun clear() {
        cancelled = false
        replacement = null
    }

    private companion object {
        val EMPTY = ByteArray(0)
    }
}

/** Whether anybody is listening at all, which is what keeps a packet from being opened for nothing. */
public interface PluginPacketBus {
    public fun listening(name: String): Boolean

    /** Runs every listener on [packet] and answers whether it should still be sent or handled. */
    public fun fire(packet: PluginPacket): Boolean

    /** The reusable packet, so firing one allocates nothing. */
    public fun packet(): PluginPacket
}

/** Sending one that the rest of the API does not cover. */
public interface PluginPacketSender {
    /** Answers false if this build has no packet by that name. */
    public fun send(
        player: PluginPlayerRef,
        name: String,
        bytes: ByteArray,
    ): Boolean

    public fun isKnown(name: String): Boolean

    public fun names(): List<String>
}
