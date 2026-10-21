package org.basaltmc.bukkit;

import org.bukkit.plugin.Plugin;
import org.bukkit.plugin.messaging.PluginMessageListener;
import org.bukkit.plugin.messaging.PluginMessageListenerRegistration;
import org.bukkit.plugin.messaging.StandardMessenger;

/** Bukkit's messenger, told to the server so that the channels a plugin listens on reach it. */
public final class BasaltMessenger extends StandardMessenger {
    @Override
    public PluginMessageListenerRegistration registerIncomingPluginChannel(Plugin plugin, String channel,
            PluginMessageListener listener) {
        PluginMessageListenerRegistration registration = super.registerIncomingPluginChannel(plugin, channel, listener);
        Natives.listenChannel(channel);
        return registration;
    }
}
