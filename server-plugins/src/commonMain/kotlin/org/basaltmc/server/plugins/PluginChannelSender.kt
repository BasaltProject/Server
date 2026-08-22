package org.basaltmc.server.plugins

/**
 * Talking to the client, or to whatever is in front of it.
 *
 * Not one of the ways plugins talk to each other: a channel goes out of this server,
 * to a mod on the client or to the proxy, and that is why it carries bytes and not values. The
 * channel name is the plugin's own business, but it has to be declared in `[capabilities] network`,
 * because a plugin listening on `bungeecord:main` is talking to the network and the operator should
 * have said yes.
 */
public interface PluginChannelSender {
    public fun send(
        player: PluginPlayerRef,
        channel: String,
        bytes: ByteArray,
    ): Boolean
}
