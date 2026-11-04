// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

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
import net.minecraft.network.chat.Component;
import net.minecraft.world.scores.TeamColor;

public class TeamColorArgument implements ArgumentType<TeamColor> {
    private static final Collection<String> EXAMPLES = Arrays.asList("red", "green");
    public static final DynamicCommandExceptionType ERROR_INVALID_VALUE = new DynamicCommandExceptionType(
        name -> Component.translatable("argument.color.invalid", name));

    private TeamColorArgument() {}

    public static TeamColorArgument teamColor() {
        return new TeamColorArgument();
    }

    public static TeamColor getTeamColor(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, TeamColor.class);
    }

    public TeamColor parse(StringReader reader) throws CommandSyntaxException {
        String name = reader.readUnquotedString();
        TeamColor color = TeamColor.byName(name);
        if (color == null) {
            throw ERROR_INVALID_VALUE.createWithContext(reader, name);
        }
        return color;
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        for (TeamColor color : TeamColor.values()) {
            if (color.getSerializedName().startsWith(builder.getRemaining())) {
                builder.suggest(color.getSerializedName());
            }
        }
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }
}
