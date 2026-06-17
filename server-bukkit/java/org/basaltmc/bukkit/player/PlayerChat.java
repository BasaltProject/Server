package org.basaltmc.bukkit.player;

import net.kyori.adventure.chat.ChatType;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer;
import net.md_5.bungee.api.chat.BaseComponent;
import net.md_5.bungee.api.chat.TextComponent;

final class PlayerChat {
    private PlayerChat() {}

    static Component component(BaseComponent... parts) {
        return LegacyComponentSerializer.legacySection().deserialize(TextComponent.toLegacyText(parts));
    }

    static Component component(BaseComponent part) {
        return part == null ? Component.empty() : component(new BaseComponent[] {part});
    }

    static Component decorate(Component message, ChatType.Bound bound) {
        if (bound == null) {
            return message;
        }
        String path = bound.type().key().value();
        Component name = bound.name();
        Component target = bound.target();
        return switch (path) {
            case "chat" -> Component.translatable("chat.type.text", name, message);
            case "say_command" -> Component.translatable("chat.type.announcement", name, message);
            case "emote_command" -> Component.translatable("chat.type.emote", name, message);
            case "msg_command_incoming" -> Component.translatable("commands.message.display.incoming", name, message);
            case "msg_command_outgoing" -> Component.translatable("commands.message.display.outgoing",
                target == null ? name : target, message);
            case "team_msg_command_incoming" -> Component.translatable("chat.type.team.text", target == null
                ? name : target, name, message);
            case "team_msg_command_outgoing" -> Component.translatable("chat.type.team.sent", target == null
                ? name : target, name, message);
            default -> message;
        };
    }
}
