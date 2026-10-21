package org.basaltmc.bukkit;

import java.util.function.Consumer;
import net.kyori.adventure.text.Component;
import net.md_5.bungee.api.ChatMessageType;
import net.md_5.bungee.api.chat.BaseComponent;
import org.bukkit.entity.Player;

final class SpigotText {
    private SpigotText() {}

    static Component component(BaseComponent... parts) {
        return net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer.legacySection()
            .deserialize(net.md_5.bungee.api.chat.TextComponent.toLegacyText(parts));
    }

    static Player.Spigot player(Consumer<Component> send, Player.Spigot base) {
        return new Player.Spigot() {
            @Override
            public void sendMessage(BaseComponent component) {
                send.accept(component(component));
            }

            @Override
            public void sendMessage(BaseComponent... components) {
                send.accept(component(components));
            }

            @Override
            public void sendMessage(ChatMessageType position, BaseComponent component) {
                send.accept(component(component));
            }

            @Override
            public void sendMessage(ChatMessageType position, BaseComponent... components) {
                send.accept(component(components));
            }

            @Override
            public void sendMessage(ChatMessageType position, java.util.UUID sender, BaseComponent component) {
                send.accept(component(component));
            }

            @Override
            public void sendMessage(ChatMessageType position, java.util.UUID sender, BaseComponent... components) {
                send.accept(component(components));
            }

            @Override
            public int getPing() {
                return 0;
            }
        };
    }

    static org.bukkit.command.CommandSender.Spigot console(Consumer<Component> send) {
        return new org.bukkit.command.CommandSender.Spigot() {
            @Override
            public void sendMessage(BaseComponent component) {
                send.accept(component(component));
            }

            @Override
            public void sendMessage(BaseComponent... components) {
                send.accept(component(components));
            }

            @Override
            public void sendMessage(java.util.UUID sender, BaseComponent component) {
                send.accept(component(component));
            }

            @Override
            public void sendMessage(java.util.UUID sender, BaseComponent... components) {
                send.accept(component(components));
            }
        };
    }
}
