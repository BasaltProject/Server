// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
public class TriggerCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "trigger");
    }
    public static CompletableFuture<Suggestions> suggestObjectives(CommandSourceStack source, SuggestionsBuilder builder) {
        java.util.List<String> names = new java.util.ArrayList<>();
        var holder = source.getEntity();
        if (holder != null) {
            var board = source.getServer().getScoreboard();
            for (var objective : board.getObjectives()) {
                var info = objective.getCriteria() == net.minecraft.world.scores.criteria.ObjectiveCriteria.TRIGGER ? board.getPlayerScoreInfo(holder, objective) : null;
                if (info != null && !info.isLocked()) {
                    names.add(objective.getName());
                }
            }
        }
        return net.minecraft.commands.SharedSuggestionProvider.suggest(names, builder);
    }
}
