// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import net.minecraft.commands.CommandSourceStack;
public class SeedCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, boolean checkPermissions) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "seed");
    }
}
