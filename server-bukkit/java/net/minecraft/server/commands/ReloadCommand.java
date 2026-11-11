// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.CommandDispatcher;
import java.util.Collection;
import net.minecraft.commands.CommandSourceStack;
public class ReloadCommand {
    public static void reloadPacks(Collection<String> selectedPacks, CommandSourceStack source) {
        source.getServer().reloadResources(selectedPacks).exceptionally(failure -> {
            source.sendFailure(Component.translatable("commands.reload.failure"));
            return null;
        });
    }
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "reload");
    }
}
