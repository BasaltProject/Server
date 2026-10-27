package org.basaltmc.bukkit.player;

import java.util.Map;
import java.util.WeakHashMap;
import org.bukkit.Bukkit;
import org.bukkit.conversations.Conversation;
import org.bukkit.entity.Player;
import org.bukkit.event.Event;
import org.bukkit.event.EventPriority;
import org.bukkit.event.Listener;
import org.bukkit.event.player.AsyncPlayerChatEvent;
import org.bukkit.plugin.Plugin;

final class ConversationRouter implements Listener {
    private static final Map<Plugin, ConversationRouter> kWatching = new WeakHashMap<>();

    private ConversationRouter() {}

    static void watch(Conversation conversation) {
        Plugin plugin = conversation.getContext().getPlugin();
        if (plugin == null) {
            return;
        }
        ConversationRouter router = new ConversationRouter();
        synchronized (kWatching) {
            ConversationRouter known = kWatching.get(plugin);
            if (known != null && known.listening(plugin)) {
                return;
            }
            kWatching.put(plugin, router);
        }
        Bukkit.getPluginManager().registerEvent(AsyncPlayerChatEvent.class, router, EventPriority.LOWEST,
            (listener, event) -> router.chat((AsyncPlayerChatEvent) event), plugin);
        Bukkit.getPluginManager().registerEvent(org.bukkit.event.server.ServerCommandEvent.class, router,
            EventPriority.LOWEST, (listener, event) -> router.console((org.bukkit.event.server.ServerCommandEvent) event),
            plugin);
        try {
            @SuppressWarnings("unchecked")
            Class<? extends Event> modern = (Class<? extends Event>) Class.forName(
                "io.papermc.paper.event.player.AsyncChatEvent");
            Bukkit.getPluginManager().registerEvent(modern, router, EventPriority.LOWEST,
                (listener, event) -> router.modern(event), plugin);
        } catch (ClassNotFoundException e) {
            return;
        }
    }

    private boolean listening(Plugin plugin) {
        for (org.bukkit.plugin.RegisteredListener registered : AsyncPlayerChatEvent.getHandlerList()
                .getRegisteredListeners(plugin)) {
            if (registered.getListener() == this) {
                return true;
            }
        }
        return false;
    }

    private void chat(AsyncPlayerChatEvent event) {
        Player player = event.getPlayer();
        if (player.isConversing()) {
            event.setCancelled(true);
            player.acceptConversationInput(event.getMessage());
        }
    }

    private void console(org.bukkit.event.server.ServerCommandEvent event) {
        org.bukkit.command.ConsoleCommandSender console = Bukkit.getConsoleSender();
        if (event.getSender() == console && console.isConversing()) {
            event.setCancelled(true);
            console.acceptConversationInput(event.getCommand());
        }
    }

    private void modern(Event event) {
        if (event instanceof io.papermc.paper.event.player.AsyncChatEvent chat && chat.getPlayer().isConversing()) {
            chat.setCancelled(true);
            chat.getPlayer().acceptConversationInput(net.kyori.adventure.text.serializer.plain
                .PlainTextComponentSerializer.plainText().serialize(chat.message()));
        }
    }
}
