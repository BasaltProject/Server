package org.basaltmc.bukkit.player;

import java.util.HashSet;
import java.util.Set;
import org.bukkit.Bukkit;
import org.bukkit.conversations.Conversation;
import org.bukkit.entity.Player;
import org.bukkit.event.Event;
import org.bukkit.event.EventPriority;
import org.bukkit.event.Listener;
import org.bukkit.event.player.AsyncPlayerChatEvent;
import org.bukkit.plugin.Plugin;

final class ConversationRouter implements Listener {
    private static final Set<Plugin> kWatching = new HashSet<>();

    private ConversationRouter() {}

    static void watch(Conversation conversation) {
        Plugin plugin = conversation.getContext().getPlugin();
        if (plugin == null) {
            return;
        }
        synchronized (kWatching) {
            if (!kWatching.add(plugin)) {
                return;
            }
        }
        ConversationRouter router = new ConversationRouter();
        Bukkit.getPluginManager().registerEvent(AsyncPlayerChatEvent.class, router, EventPriority.LOWEST,
            (listener, event) -> router.chat((AsyncPlayerChatEvent) event), plugin);
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

    private void chat(AsyncPlayerChatEvent event) {
        Player player = event.getPlayer();
        if (player.isConversing()) {
            event.setCancelled(true);
            player.acceptConversationInput(event.getMessage());
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
