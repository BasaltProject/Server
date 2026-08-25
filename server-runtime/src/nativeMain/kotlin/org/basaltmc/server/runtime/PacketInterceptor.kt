package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.plugins.PacketNames
import org.basaltmc.wire.io.ByteBuffer

/**
 * Shows [ClientSession]'s packets to whoever is watching them on [ClientSession.pluginPackets].
 *
 * A back-reference to the owning session rather than captured values: [ClientSession.state] and
 * [ClientSession.playerSession] both change over the life of the connection, and this has to see
 * the current one, not the one at construction time.
 */
internal class PacketInterceptor(
    private val owner: ClientSession,
) {
    /**
     * Shows an incoming packet to whoever is watching it, and answers whether to handle it.
     *
     * The bytes are read without consuming them, so a listener seeing a packet costs a copy and the
     * server still decodes it exactly as it would have. Replacing them puts the new body in the same
     * buffer, which is what makes changing one on the way in possible at all before the fields
     * themselves are named.
     */
    internal fun interceptIncoming(
        packetId: Int,
        body: ByteBuffer,
    ): Boolean {
        val bus = owner.pluginPackets ?: return true
        val name = PacketNames.of(owner.state, serverbound = true, id = packetId)
        if (!bus.listening(name)) return true
        val session = owner.playerSession
        val packet = bus.packet()
        packet.name = name
        packet.serverbound = true
        packet.playerName = session?.name.orEmpty()
        packet.playerUuid = session?.uuid?.toString().orEmpty()
        packet.bytes = body.toByteArray()
        if (!bus.fire(packet)) return false
        packet.replacement?.let {
            body.clear()
            body.writeBytes(it)
        }
        return true
    }

    /**
     * Shows an outgoing packet to whoever is watching it, and answers with what to send.
     *
     * Null means a plugin cancelled it. The first line is the whole performance story: with nobody
     * listening to that name this is one map lookup and the bytes are never copied.
     */
    internal fun interceptOutgoing(
        id: Int,
        bytes: ByteArray,
        idBytes: Int,
    ): ByteArray? {
        val bus = owner.pluginPackets ?: return bytes
        val name = PacketNames.of(owner.state, serverbound = false, id = id)
        if (!bus.listening(name)) return bytes
        val session = owner.playerSession
        val packet = bus.packet()
        packet.name = name
        packet.serverbound = false
        packet.playerName = session?.name.orEmpty()
        packet.playerUuid = session?.uuid?.toString().orEmpty()
        packet.bytes = bytes.copyOfRange(idBytes, bytes.size)
        if (!bus.fire(packet)) return null
        val replaced = packet.replacement ?: return bytes
        return bytes.copyOfRange(0, idBytes) + replaced
    }
}
