package org.basaltmc.bukkit.facade;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.arguments.StringArgumentType;
import com.mojang.brigadier.context.CommandContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.Commands;
import net.minecraft.network.chat.Component;
import org.basaltmc.bukkit.Natives;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.command.CommandSender;
import org.bukkit.entity.Player;

/** Vanilla command roots that run in the native command service. */
public final class NativeCommands {
    private static final String kRest = "arguments";

    private NativeCommands() {}

    public static int run(CommandSourceStack source, String line) {
        org.basaltmc.bukkit.BasaltPlayer.invalidateCaches();
        CommandSender sender = BukkitCommandBridge.senderOf(source);
        boolean ok = sender instanceof Player player
            ? WorldNatives.playerCommand(player.getUniqueId().toString(), line)
            : Natives.runConsoleCommand(line);
        if (!ok) {
            source.sendFailure(Component.translatable("command.failed"));
        }
        return ok ? 1 : 0;
    }

    public static void delegate(CommandDispatcher<CommandSourceStack> dispatcher, String... roots) {
        for (String root : roots) {
            if (dispatcher.getRoot().getChild(root) == null) {
                dispatcher.register(Commands.literal(root)
                    .executes(context -> run(context.getSource(), root))
                    .then(Commands.argument(kRest, StringArgumentType.greedyString())
                        .executes(context -> run(context.getSource(), root + " " + rest(context)))));
            }
        }
    }

    private static String rest(CommandContext<CommandSourceStack> context) {
        return StringArgumentType.getString(context, kRest);
    }
}
