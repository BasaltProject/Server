package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.LuaFailure

/** Who is listening on which channel, by channel name. */
internal class PluginChannelHost {
    /** One plugin listening on one channel. */
    private class ChannelListener(
        val plugin: LoadedPlugin,
        val fn: LuaValue,
    )

    private val channels = LinkedHashMap<String, MutableList<ChannelListener>>()

    /** `channel("arena:main", fn)`: what arrives on a channel from this player's client. */
    internal fun listenOn(
        plugin: LoadedPlugin,
        channel: String,
        fn: LuaValue,
    ) {
        val list = channels.getOrPut(channel) { mutableListOf() }
        val entry = ChannelListener(plugin, fn)
        list += entry
        plugin.registrations.add("channel($channel)") {
            list.remove(entry)
            if (list.isEmpty()) channels.remove(channel)
        }
    }

    internal fun listeningOnChannel(channel: String): Boolean = channels.containsKey(channel)

    /** A payload arrived: hands the bytes to whoever asked for that channel. */
    internal fun channelMessage(
        host: PluginHost,
        channel: String,
        playerUuid: String,
        bytes: ByteArray,
    ) {
        val listeners = channels[channel] ?: return
        val player = host.services.players.byUuid(playerUuid)
        for (entry in listeners.toList()) {
            val plugin = entry.plugin
            if (plugin.disabled) continue
            val instance = plugin.instances.firstOrNull() ?: continue
            instance.sandbox.refill()
            instance.deciderDepth++
            try {
                val list = Lua.table()
                for ((index, byte) in bytes.withIndex()) list.set(index + 1, Lua.of(byte.toInt() and 0xff))
                entry.fn.call(
                    player?.let {
                        playerObject(it, host.services.storage, plugin.manifest.id, plugin.text)
                    } ?: Lua.NIL,
                    list,
                )
                plugin.strikes = 0
            } catch (e: net.blueva.luak.LuaError) {
                host.report(plugin, LuaFailure(plugin.manifest.id, e), "channel $channel")
            } finally {
                instance.deciderDepth--
            }
        }
    }
}
