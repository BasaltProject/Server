// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import net.minecraft.server.level.ServerPlayer;
public interface OutgoingChatMessage {
    Component content();
    void sendToPlayer(ServerPlayer player, boolean filtered, ChatType.Bound chatType);
    static OutgoingChatMessage create(PlayerChatMessage message) {
        return message.isSystem() ? new Disguised(message.decoratedContent()) : new Player(message);
    }
    public static record Player(PlayerChatMessage message) implements OutgoingChatMessage {
        public Component content() {
            return message.decoratedContent();
        }
        public void sendToPlayer(ServerPlayer player, boolean filtered, ChatType.Bound chatType) {
            PlayerChatMessage filteredMessage = message.filter(filtered);
            if (!filteredMessage.isFullyFiltered()) {
                player.connection.sendPlayerChatMessage(filteredMessage, chatType);
            }
        }
    }
    public static record Disguised(Component content) implements OutgoingChatMessage {
        public void sendToPlayer(ServerPlayer player, boolean filtered, ChatType.Bound chatType) {
            player.connection.sendDisguisedChatMessage(content, chatType);
        }
    }
}
