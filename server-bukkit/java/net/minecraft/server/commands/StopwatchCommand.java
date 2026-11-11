// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.SuggestionProvider;
import net.minecraft.commands.CommandSourceStack;
public class StopwatchCommand {
    public static final DynamicCommandExceptionType ERROR_DOES_NOT_EXIST = new DynamicCommandExceptionType(id -> net.minecraft.network.chat.Component.translatableEscape("commands.stopwatch.does_not_exist", id));
    public static final SuggestionProvider<CommandSourceStack> SUGGEST_STOPWATCHES = (c, p) -> net.minecraft.commands.SharedSuggestionProvider.suggestResource(c.getSource().getServer().getStopwatches().ids(), p);
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "stopwatch");
    }
}
