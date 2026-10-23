package org.basaltmc.bukkit.player;

import java.net.InetSocketAddress;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;
import net.md_5.bungee.api.ChatMessageType;
import net.md_5.bungee.api.chat.BaseComponent;
import org.bukkit.Bukkit;
import org.bukkit.entity.Player;

final class PlayerSpigot extends Player.Spigot {
    private final Player player;

    PlayerSpigot(Player player) {
        this.player = player;
    }

    @Override
    public InetSocketAddress getRawAddress() {
        return player.getAddress();
    }

    @Override
    public void respawn() {
        if (player.isDead()) {
            PlayerCalls.call(player.getUniqueId(), "respawn");
        }
    }

    @Override
    public Set<Player> getHiddenPlayers() {
        Set<Player> hidden = new HashSet<>();
        for (Player other : Bukkit.getOnlinePlayers()) {
            if (!player.canSee(other)) {
                hidden.add(other);
            }
        }
        return hidden;
    }

    @Override
    public void sendMessage(BaseComponent component) {
        player.sendMessage(PlayerChat.component(component));
    }

    @Override
    public void sendMessage(BaseComponent... components) {
        player.sendMessage(PlayerChat.component(components));
    }

    @Override
    public void sendMessage(ChatMessageType position, BaseComponent component) {
        sendMessage(position, new BaseComponent[] {component});
    }

    @Override
    public void sendMessage(ChatMessageType position, BaseComponent... components) {
        if (position == ChatMessageType.ACTION_BAR) {
            player.sendActionBar(PlayerChat.component(components));
        } else {
            player.sendMessage(PlayerChat.component(components));
        }
    }

    @Override
    public void sendMessage(ChatMessageType position, UUID sender, BaseComponent component) {
        sendMessage(position, component);
    }

    @Override
    public void sendMessage(ChatMessageType position, UUID sender, BaseComponent... components) {
        sendMessage(position, components);
    }

    @Override
    public int getPing() {
        return player.getPing();
    }
}
