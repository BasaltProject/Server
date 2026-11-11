// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import java.util.Map;
import net.minecraft.network.chat.PlayerChatMessage;
public interface CommandSigningContext {
    CommandSigningContext ANONYMOUS = name -> null;
    PlayerChatMessage getArgument(String name);
    public static record SignedArguments(Map<String, PlayerChatMessage> arguments) implements CommandSigningContext {
        public PlayerChatMessage getArgument(String name) {
            return arguments.get(name);
        }
    }
}
