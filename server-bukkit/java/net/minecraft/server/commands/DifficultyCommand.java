// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import net.minecraft.server.MinecraftServer;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.Difficulty;
public class DifficultyCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "difficulty");
    }
    public static int setDifficulty(CommandSourceStack source, Difficulty difficulty) throws CommandSyntaxException {
        MinecraftServer server = source.getServer();
        if (server.getWorldData().getDifficulty() == difficulty) {
            throw new com.mojang.brigadier.exceptions.DynamicCommandExceptionType(name -> Component.translatable("commands.difficulty.failure", name))
                .create(difficulty.getDisplayName());
        }
        server.setDifficulty(difficulty, true);
        source.sendSuccess(() -> Component.translatable("commands.difficulty.success", difficulty.getDisplayName()), true);
        return 0;
    }
}
