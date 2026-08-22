package org.basaltmc.server.plugins

import net.blueva.luak.LuaError
import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/**
 * A packet as Lua sees it.
 *
 * Deliberately thin: name, who it is about, the bytes, and whether it goes on. The fields of each
 * packet by name are the next thing this grows, and until then `bytes` is the honest answer rather
 * than a half-decoded table that would be wrong for most of the hundred and thirty six.
 */
internal class PluginPacketView(
    private val packet: PluginPacket,
    private val players: PluginPlayers,
    private val host: PluginHost,
    private val plugin: LoadedPlugin,
) {
    val table: LuaTable = Lua.table()

    init {
        val metatable = Lua.table()
        metatable.set("__index", HostFunction("packet.index") { args -> read(args) })
        metatable.set("__newindex", HostFunction("packet.newindex") { args -> write(args) })
        table.setmetatable(metatable)
    }

    private fun read(args: Varargs): Varargs =
        when (val key = args.arg(2).tojstring()) {
            "name" -> Lua.of(packet.name)
            "serverbound" -> Lua.of(packet.serverbound)
            "size" -> Lua.of(packet.bytes.size)
            "player" ->
                players.byUuid(packet.playerUuid)?.let {
                    playerObject(it, host.services.storage, plugin.manifest.id, plugin.text)
                } ?: Lua.NIL

            "player_name" -> Lua.of(packet.playerName)
            "player_uuid" -> Lua.of(packet.playerUuid)
            "cancelled" -> Lua.of(packet.cancelled)
            "bytes" -> bytesTable()
            "cancel" -> cancelFunction
            else ->
                throw LuaError(
                    "a packet has no '$key'; it has name, serverbound, player, player_name, " +
                        "player_uuid, size, bytes, cancelled and cancel()",
                )
        }

    private fun write(args: Varargs): Varargs {
        when (val key = args.arg(2).tojstring()) {
            "cancelled" -> packet.cancelled = args.arg(3).toboolean()
            "bytes" -> packet.replacement = bytesFrom(args.arg(3))
            else -> throw LuaError("'$key' of a packet cannot be changed; only 'cancelled' and 'bytes' can")
        }
        return Lua.NONE
    }

    /** One-based, like every other list in Lua, and a copy: the packet's own array is reused. */
    private fun bytesTable(): LuaTable {
        val out = Lua.table()
        for ((index, byte) in packet.bytes.withIndex()) out.set(index + 1, Lua.of(byte.toInt() and 0xff))
        return out
    }

    private fun bytesFrom(value: LuaValue): ByteArray {
        if (!value.istable()) throw LuaError("packet bytes are a list of numbers from 0 to 255")
        val length = value.length()
        return ByteArray(length) { index ->
            val at = value.get(index + 1) ?: Lua.NIL
            (at.toint() and 0xff).toByte()
        }
    }

    private val cancelFunction: LuaValue =
        HostFunction("packet.cancel") {
            packet.cancelled = true
            Lua.NONE
        }
}
