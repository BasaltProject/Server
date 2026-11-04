// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.scores.Objective;

public class ObjectiveArgument implements ArgumentType<String> {
    private static final Collection<String> EXAMPLES = Arrays.asList("foo", "*", "012");
    private static final DynamicCommandExceptionType ERROR_OBJECTIVE_NOT_FOUND = new DynamicCommandExceptionType(name -> new LiteralMessage("Unknown scoreboard objective '" + name + "'"));
    private static final DynamicCommandExceptionType ERROR_OBJECTIVE_READ_ONLY = new DynamicCommandExceptionType(name -> new LiteralMessage("The objective '" + name + "' is read-only and cannot be changed"));

    public static ObjectiveArgument objective() {
        return new ObjectiveArgument();
    }

    public static Objective getObjective(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        String id = context.getArgument(name, String.class);
        Objective objective = context.getSource().getServer().getScoreboard().getObjective(id);
        if (objective == null) {
            throw ERROR_OBJECTIVE_NOT_FOUND.create(id);
        }
        return objective;
    }

    public static Objective getWritableObjective(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        Objective objective = getObjective(context, name);
        if (objective.getCriteria().isReadOnly()) {
            throw ERROR_OBJECTIVE_READ_ONLY.create(objective.getName());
        }
        return objective;
    }

    public String parse(StringReader reader) throws CommandSyntaxException {
        return reader.readUnquotedString();
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        if (context.getSource() instanceof CommandSourceStack source) {
            source.getServer().getScoreboard().getObjectiveNames().stream()
                .filter(each -> each.startsWith(builder.getRemaining())).forEach(builder::suggest);
        }
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }
}
