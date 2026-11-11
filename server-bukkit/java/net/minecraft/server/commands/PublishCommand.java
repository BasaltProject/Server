// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.CommandDispatcher;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.MutableComponent;
public class PublishCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "publish");
    }
    public static MutableComponent getSuccessMessage(int port) {
        return Component.translatable("commands.publish.started.lan", net.minecraft.network.chat.ComponentUtils.copyOnClickText(String.valueOf(port)));
    }
}
