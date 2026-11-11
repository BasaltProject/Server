// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.CommandDispatcher;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.entity.Avatar;
public class FetchProfileCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "fetchprofile");
    }
    public static void printForAvatar(CommandSourceStack source, Avatar avatar) {
        source.sendSuccess(() -> Component.translatable("commands.fetchprofile.entity.success", avatar.getDisplayName(), avatar.getProfile().name()), false);
    }
}
